#include "sysinfo.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef ALL_PROCESSOR_GROUPS
#define ALL_PROCESSOR_GROUPS 0xffff
#endif

int sysinfo_cpu_cores(void) {
    /* GetActiveProcessorCount spans all NUMA groups (>64 LPs);
     * GetSystemInfo().dwNumberOfProcessors is limited to one group. */
    DWORD n = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (n < 1) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        if (si.dwNumberOfProcessors < 1) {
            return 1;
        }
        return (int)si.dwNumberOfProcessors;
    }
    return (int)n;
}

long long sysinfo_mem_total(void) {
    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if (!GlobalMemoryStatusEx(&st)) {
        return -1;
    }
    return (long long)st.ullTotalPhys;
}

long long sysinfo_mem_avail(void) {
    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if (!GlobalMemoryStatusEx(&st)) {
        return -1;
    }
    return (long long)st.ullAvailPhys;
}

static char g_ver[160];

const char *sysinfo_os_version(void) {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll != 0) {
        typedef LONG (WINAPI *RtlGetVersionFn)(void *info);
        RtlGetVersionFn fn = (RtlGetVersionFn)GetProcAddress(ntdll, "RtlGetVersion");
        if (fn != 0) {
            struct WinVer {
                unsigned long size;
                unsigned long major;
                unsigned long minor;
                unsigned long build;
                unsigned long plat;
                wchar_t csd[128];
            } vi;
            memset(&vi, 0, sizeof(vi));
            vi.size = sizeof(vi);
            if (fn(&vi) == 0) {
                const char *label = "Windows";
                if (vi.major == 10 && vi.build >= 22000) {
                    label = "Windows 11";
                } else if (vi.major == 10) {
                    label = "Windows 10";
                } else if (vi.major == 6 && vi.minor == 3) {
                    label = "Windows 8.1";
                } else if (vi.major == 6 && vi.minor == 1) {
                    label = "Windows 7";
                }
                /* Enrich with registry EditionID/DisplayVersion; the version
                 * label stays authoritative because ProductName can lag it
                 * (e.g. "Windows 10 Education" on 11-series builds).
                 * Server SKUs get build-mapped labels (2016/2019/2022/2025). */
                {
                    char edition[64] = { 0 };
                    char display[64] = { 0 };
                    HKEY hk;
                    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                            "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                            0, KEY_READ, &hk) == ERROR_SUCCESS) {
                        DWORD sz = sizeof(edition), t = 0;
                        if (RegQueryValueExA(hk, "EditionID", 0, &t,
                                (LPBYTE)edition, &sz) != ERROR_SUCCESS) {
                            edition[0] = '\0';
                        }
                        sz = sizeof(display);
                        t = 0;
                        if (RegQueryValueExA(hk, "DisplayVersion", 0, &t,
                                (LPBYTE)display, &sz) != ERROR_SUCCESS) {
                            display[0] = '\0';
                        }
                        RegCloseKey(hk);
                    }
                    edition[sizeof(edition) - 1] = '\0';
                    display[sizeof(display) - 1] = '\0';
                    if (strncmp(edition, "Server", 6) == 0) {
                        if (vi.build >= 26100) {
                            label = "Windows Server 2025";
                        } else if (vi.build >= 20348) {
                            label = "Windows Server 2022";
                        } else if (vi.build >= 17763) {
                            label = "Windows Server 2019";
                        } else if (vi.build >= 14393) {
                            label = "Windows Server 2016";
                        } else {
                            label = "Windows Server";
                        }
                    }
                    if (edition[0] != '\0' && display[0] != '\0') {
                        snprintf(g_ver, sizeof(g_ver), "%s %s %s (%lu.%lu.%lu)",
                            label, edition, display, vi.major, vi.minor, vi.build);
                    } else if (edition[0] != '\0') {
                        snprintf(g_ver, sizeof(g_ver), "%s %s (%lu.%lu.%lu)",
                            label, edition, vi.major, vi.minor, vi.build);
                    } else {
                        snprintf(g_ver, sizeof(g_ver), "%s (%lu.%lu.%lu)",
                            label, vi.major, vi.minor, vi.build);
                    }
                }
                return g_ver;
            }
        }
    }
    return "Windows";
}

static char g_cpu[256];

const char *sysinfo_cpu_model(void) {
    HKEY h;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0, KEY_READ, &h) == ERROR_SUCCESS) {
        char name[256];
        DWORD sz = sizeof(name), t = 0;
        if (RegQueryValueExA(h, "ProcessorNameString", 0, &t,
                (LPBYTE)name, &sz) == ERROR_SUCCESS && sz > 1) {
            RegCloseKey(h);
            if (sz >= sizeof(name)) {
                sz = sizeof(name) - 1;
            }
            name[sz] = '\0';
            /* Trim trailing spaces left in the registry string. */
            size_t len = strlen(name);
            while (len > 0 && (name[len - 1] == ' ' || name[len - 1] == '\t')) {
                name[--len] = '\0';
            }
            if (name[0] != '\0') {
                strncpy(g_cpu, name, sizeof(g_cpu) - 1);
                g_cpu[sizeof(g_cpu) - 1] = '\0';
                return g_cpu;
            }
            return "";
        }
        RegCloseKey(h);
    }
    /* Fallback to env var only if the registry read fails. */
    const char *s = getenv("PROCESSOR_IDENTIFIER");
    if (s == 0 || s[0] == '\0') {
        return "";
    }
    strncpy(g_cpu, s, sizeof(g_cpu) - 1);
    g_cpu[sizeof(g_cpu) - 1] = '\0';
    return g_cpu;
}

static char g_host[256];

const char *sysinfo_host_name(void) {
    DWORD n = sizeof(g_host);
    if (!GetComputerNameA(g_host, &n)) {
        return "";
    }
    return g_host;
}

/* Current Unix time (seconds). Shared helper so boot_unix and uptime
 * stay internally consistent: boot = now - uptime. */
static long long win32_unix_now(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER t;
    t.LowPart = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    return (long long)(t.QuadPart / 10000000ULL) - 11644473600LL;
}

long long sysinfo_uptime_sec(void) {
    return (long long)(GetTickCount64() / 1000);
}

long long sysinfo_disk_total(const char *path) {
    ULARGE_INTEGER avail, total, free_b;
    const char *p = (path != 0 && path[0] != '\0') ? path : "C:\\";
    if (!GetDiskFreeSpaceExA(p, &avail, &total, &free_b)) {
        return -1;
    }
    return (long long)total.QuadPart;
}

long long sysinfo_disk_free(const char *path) {
    ULARGE_INTEGER avail, total, free_b;
    const char *p = (path != 0 && path[0] != '\0') ? path : "C:\\";
    if (!GetDiskFreeSpaceExA(p, &avail, &total, &free_b)) {
        return -1;
    }
    return (long long)avail.QuadPart;
}

static char g_tz[128];

const char *sysinfo_tz_name(void) {
    DYNAMIC_TIME_ZONE_INFORMATION tz;
    memset(&tz, 0, sizeof(tz));
    if (GetDynamicTimeZoneInformation(&tz) == TIME_ZONE_ID_INVALID) {
        return "";
    }
    int i = 0;
    while (i < 127 && tz.TimeZoneKeyName[i] != 0) {
        g_tz[i] = (char)tz.TimeZoneKeyName[i];
        i++;
    }
    g_tz[i] = '\0';
    return g_tz;
}

int sysinfo_utc_offset_min(void) {
    TIME_ZONE_INFORMATION tzi;
    memset(&tzi, 0, sizeof(tzi));
    DWORD rc = GetTimeZoneInformation(&tzi);
    if (rc == TIME_ZONE_ID_INVALID) {
        /* 0 is a valid UTC offset, so failure uses a distinct sentinel. */
        return -32768;
    }
    LONG effective;
    if (rc == TIME_ZONE_ID_DAYLIGHT) {
        effective = (LONG)tzi.Bias + (LONG)tzi.DaylightBias;
    } else {
        effective = (LONG)tzi.Bias + (LONG)tzi.StandardBias;
    }
    return -(int)effective;
}

/* ---- Extended detail APIs ---- */

#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

static char g_kernel[64];

const char *sysinfo_kernel_version(void) {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll != 0) {
        typedef LONG (WINAPI *RtlGetVersionFn)(void *info);
        RtlGetVersionFn fn = (RtlGetVersionFn)GetProcAddress(ntdll, "RtlGetVersion");
        if (fn != 0) {
            struct WinVer2 {
                unsigned long size;
                unsigned long major;
                unsigned long minor;
                unsigned long build;
                unsigned long plat;
                wchar_t csd[128];
            } vi;
            memset(&vi, 0, sizeof(vi));
            vi.size = sizeof(vi);
            if (fn(&vi) == 0) {
                snprintf(g_kernel, sizeof(g_kernel), "NT %lu.%lu.%lu",
                    vi.major, vi.minor, vi.build);
                return g_kernel;
            }
        }
    }
    return "";
}

static char g_distro_id[64];
static char g_distro_ver[64];

const char *sysinfo_distro_id(void) {
    HKEY h;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            0, KEY_READ, &h) != ERROR_SUCCESS) {
        return "";
    }
    DWORD sz = sizeof(g_distro_id), t = 0;
    if (RegQueryValueExA(h, "EditionID", 0, &t,
            (LPBYTE)g_distro_id, &sz) != ERROR_SUCCESS) {
        RegCloseKey(h);
        return "";
    }
    RegCloseKey(h);
    g_distro_id[sizeof(g_distro_id) - 1] = '\0';
    return g_distro_id;
}

const char *sysinfo_distro_version(void) {
    HKEY h;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
            0, KEY_READ, &h) != ERROR_SUCCESS) {
        return "";
    }
    DWORD sz = sizeof(g_distro_ver), t = 0;
    if (RegQueryValueExA(h, "DisplayVersion", 0, &t,
            (LPBYTE)g_distro_ver, &sz) != ERROR_SUCCESS) {
        RegCloseKey(h);
        return "";
    }
    RegCloseKey(h);
    g_distro_ver[sizeof(g_distro_ver) - 1] = '\0';
    return g_distro_ver;
}

int sysinfo_cpu_physical(void) {
    DWORD len = 0;
    GetLogicalProcessorInformation(0, &len);
    if (len == 0) {
        return -1;
    }
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *buf =
        (SYSTEM_LOGICAL_PROCESSOR_INFORMATION*)malloc(len);
    if (!buf) {
        return -1;
    }
    int phys = -1;
    if (GetLogicalProcessorInformation(buf, &len)) {
        phys = 0;
        DWORD n = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        for (DWORD i = 0; i < n; i++) {
            if (buf[i].Relationship == RelationProcessorCore) {
                phys++;
            }
        }
        if (phys < 1) {
            phys = -1;
        }
    }
    free(buf);
    return phys;
}

long long sysinfo_cpu_freq_mhz(void) {
    /* Scan CentralProcessor\<n> subkeys and return the max ~MHz. */
    DWORD max_mhz = 0;
    int any = 0;
    int i = 0;
    for (i = 0; i < 64; i++) {
        char key[128];
        snprintf(key, sizeof(key),
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\%d", i);
        HKEY h;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, key,
                0, KEY_READ, &h) != ERROR_SUCCESS) {
            break; /* stop at first missing subkey */
        }
        DWORD mhz = 0, sz = sizeof(mhz), t = 0;
        if (RegQueryValueExA(h, "~MHz", 0, &t,
                (LPBYTE)&mhz, &sz) == ERROR_SUCCESS && mhz > 0) {
            any = 1;
            if (mhz > max_mhz) {
                max_mhz = mhz;
            }
        }
        RegCloseKey(h);
    }
    if (any && max_mhz > 0) {
        return (long long)max_mhz;
    }
    /* Fallback: single read of CPU0 (old behavior). */
    HKEY h;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
            0, KEY_READ, &h) != ERROR_SUCCESS) {
        return -1;
    }
    DWORD mhz = 0, sz = sizeof(mhz), t = 0;
    LONG rc = RegQueryValueExA(h, "~MHz", 0, &t, (LPBYTE)&mhz, &sz);
    RegCloseKey(h);
    if (rc != ERROR_SUCCESS || mhz == 0) {
        return -1;
    }
    return (long long)mhz;
}

/* 12-char CPUID vendor string + NUL; 32 bytes for headroom. */
static char g_vendor[32];

const char *sysinfo_cpu_vendor(void) {
#if defined(__i386__) || defined(__x86_64__)
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (__get_cpuid(0, &a, &b, &c, &d)) {
        memcpy(g_vendor, &b, 4);
        memcpy(g_vendor + 4, &d, 4);
        memcpy(g_vendor + 8, &c, 4);
        g_vendor[12] = '\0';
        return g_vendor;
    }
#endif
    /* No cpuid path (e.g. ARM64) or detection failed: return "".
     * PROCESSOR_ARCHITECTURE lives in a different namespace, so it
     * must not be reported as the CPU vendor. */
    return "";
}

long long sysinfo_swap_total(void) {
    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if (!GlobalMemoryStatusEx(&st)) {
        return -1;
    }
    long long total = (long long)st.ullTotalPageFile - (long long)st.ullTotalPhys;
    return total < 0 ? 0 : total;
}

long long sysinfo_swap_avail(void) {
    MEMORYSTATUSEX st;
    st.dwLength = sizeof(st);
    if (!GlobalMemoryStatusEx(&st)) {
        return -1;
    }
    long long avail = (long long)st.ullAvailPageFile - (long long)st.ullAvailPhys;
    if (avail < 0) {
        return 0;
    }
    long long total = (long long)st.ullTotalPageFile - (long long)st.ullTotalPhys;
    if (total >= 0 && avail > total) {
        return total;
    }
    return avail;
}

int sysinfo_battery_percent(void) {
    SYSTEM_POWER_STATUS sps;
    memset(&sps, 0, sizeof(sps));
    if (!GetSystemPowerStatus(&sps)) {
        return -1;
    }
    if (sps.BatteryFlag & 128) {
        return -1;
    }
    if (sps.BatteryLifePercent > 100) {
        return -1;
    }
    return (int)sps.BatteryLifePercent;
}

int sysinfo_on_ac(void) {
    SYSTEM_POWER_STATUS sps;
    memset(&sps, 0, sizeof(sps));
    if (!GetSystemPowerStatus(&sps)) {
        return -1;
    }
    if (sps.ACLineStatus > 1) {
        return -1;
    }
    return (int)sps.ACLineStatus;
}

static char g_exe[1024];

const char *sysinfo_exe_path(void) {
    WCHAR w[1024];
    DWORD n = GetModuleFileNameW(0, w, sizeof(w) / sizeof(w[0]));
    if (n == 0 || n >= sizeof(w) / sizeof(w[0])) {
        return "";
    }
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1,
            g_exe, (int)sizeof(g_exe), 0, 0) == 0) {
        return "";
    }
    return g_exe;
}

static char g_env[65536];
static int g_env_truncated = 0;

const char *sysinfo_env_block(void) {
    g_env_truncated = 0;
    char *block = GetEnvironmentStringsA();
    if (block == 0) {
        return "";
    }
    size_t pos = 0;
    const char *p = block;
    while (*p != '\0' && pos + 1 < sizeof(g_env)) {
        size_t n = strlen(p);
        if (pos + n + 1 >= sizeof(g_env)) {
            g_env_truncated = 1;
            break;
        }
        memcpy(g_env + pos, p, n);
        pos += n;
        g_env[pos++] = '\n';
        p += n + 1;
    }
    g_env[pos] = '\0';
    if (*p != '\0') {
        /* Loop exited on a full buffer with entries left over. */
        g_env_truncated = 1;
    }
    FreeEnvironmentStringsA(block);
    return g_env;
}

int sysinfo_env_truncated(void) {
    return g_env_truncated;
}

long long sysinfo_boot_unix(void) {
    /* boot + uptime == now (uptime is truncated to whole seconds). */
    return win32_unix_now() - sysinfo_uptime_sec();
}

double sysinfo_load_1(void) {
    return -1.0;
}

double sysinfo_load_5(void) {
    return -1.0;
}

double sysinfo_load_15(void) {
    return -1.0;
}

int sysinfo_mount_count(void) {
    DWORD need = GetLogicalDriveStringsA(0, 0);
    if (need <= 1) {
        return 0;
    }
    char *buf = (char*)malloc(need);
    if (!buf) {
        return 0;
    }
    DWORD got = GetLogicalDriveStringsA(need, buf);
    int count = 0;
    const char *p = buf;
    while (*p != '\0' && (DWORD)(p - buf) < got) {
        count++;
        p += strlen(p) + 1;
    }
    free(buf);
    return count;
}

static char g_mount[512];

const char *sysinfo_mount_at(int index) {
    if (index < 0) {
        return "";
    }
    DWORD need = GetLogicalDriveStringsA(0, 0);
    if (need <= 1) {
        return "";
    }
    char *buf = (char*)malloc(need);
    if (!buf) {
        return "";
    }
    DWORD got = GetLogicalDriveStringsA(need, buf);
    const char *p = buf;
    int i = 0;
    const char *found = 0;
    while (*p != '\0' && (DWORD)(p - buf) < got) {
        if (i == index) {
            found = p;
            break;
        }
        i++;
        p += strlen(p) + 1;
    }
    if (!found) {
        free(buf);
        return "";
    }
    char fs[64] = { 0 };
    if (!GetVolumeInformationA(found, 0, 0, 0, 0, 0, fs, sizeof(fs))) {
        /* Strict: no fs info means no usable mount entry. */
        free(buf);
        return "";
    }
    snprintf(g_mount, sizeof(g_mount), "%s|%s", found, fs);
    free(buf);
    return g_mount;
}

static char g_fs[64];

const char *sysinfo_fs_type(const char *path) {
    const char *p = (path && path[0]) ? path : "C:\\";
    g_fs[0] = '\0';
    GetVolumeInformationA(p, 0, 0, 0, 0, 0, g_fs, sizeof(g_fs));
    return g_fs;
}

int sysinfo_is_elevated(void) {
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    PSID admin = 0;
    if (!AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin)) {
        return -1;
    }
    BOOL member = FALSE;
    if (!CheckTokenMembership(0, admin, &member)) {
        FreeSid(admin);
        return -1;
    }
    FreeSid(admin);
    return member ? 1 : 0;
}

static char g_user[256];

const char *sysinfo_user_name(void) {
    WCHAR w[256];
    DWORD n = sizeof(w) / sizeof(w[0]);
    if (!GetUserNameW(w, &n)) {
        return "";
    }
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1,
            g_user, (int)sizeof(g_user), 0, 0) == 0) {
        return "";
    }
    return g_user;
}

int sysinfo_mem_estimated(void) {
    /* Windows memory counters are exact, never estimated. */
    return 0;
}

static char g_host_native[256];

const char *sysinfo_host_native(void) {
    WCHAR w[256];
    DWORD n = sizeof(w) / sizeof(w[0]);
    if (!GetComputerNameW(w, &n)) {
        return "";
    }
    if (WideCharToMultiByte(CP_UTF8, 0, w, -1,
            g_host_native, (int)sizeof(g_host_native), 0, 0) == 0) {
        return "";
    }
    return g_host_native;
}

/* Enumerate adapters via GetAdaptersAddresses. Returns a malloc'd list
 * the caller must free, or 0 on any failure. */
static IP_ADAPTER_ADDRESSES *win32_net_enum(void) {
    ULONG size = 0;
    DWORD rc;
    IP_ADAPTER_ADDRESSES *list;
    rc = GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        0, 0, &size);
    (void)rc;
    if (size == 0) {
        return 0;
    }
    list = (IP_ADAPTER_ADDRESSES*)malloc(size);
    if (!list) {
        return 0;
    }
    rc = GetAdaptersAddresses(AF_UNSPEC,
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
        0, list, &size);
    if (rc == ERROR_BUFFER_OVERFLOW) {
        /* Retry once with the new size. */
        IP_ADAPTER_ADDRESSES *retry;
        free(list);
        if (size == 0) {
            return 0;
        }
        retry = (IP_ADAPTER_ADDRESSES*)malloc(size);
        if (!retry) {
            return 0;
        }
        list = retry;
        rc = GetAdaptersAddresses(AF_UNSPEC,
            GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            0, list, &size);
    }
    if (rc != NO_ERROR) {
        free(list);
        return 0;
    }
    return list;
}

int sysinfo_net_count(void) {
    IP_ADAPTER_ADDRESSES *list = win32_net_enum();
    IP_ADAPTER_ADDRESSES *a;
    int n = 0;
    if (!list) {
        return -1;
    }
    for (a = list; a != 0; a = a->Next) {
        n++;
    }
    free(list);
    return n;
}

static char g_net[512];

const char *sysinfo_net_at(int index) {
    IP_ADAPTER_ADDRESSES *list;
    IP_ADAPTER_ADDRESSES *a = 0;
    PIP_ADAPTER_UNICAST_ADDRESS ua;
    char name[256] = { 0 };
    char mac[32] = { 0 };
    char ipv4[64] = { 0 };
    char ipv6[64] = { 0 };
    const char *up;
    const char *loop;
    int i = 0;
    WSADATA wsa;
    if (index < 0) {
        return "";
    }
    list = win32_net_enum();
    if (!list) {
        return "";
    }
    for (a = list; a != 0; a = a->Next) {
        if (i == index) {
            break;
        }
        i++;
    }
    if (!a) {
        free(list);
        return "";
    }
    /* FriendlyName -> UTF-8. */
    if (a->FriendlyName != 0) {
        if (WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1,
                name, (int)sizeof(name), 0, 0) == 0) {
            name[0] = '\0';
        } else {
            name[sizeof(name) - 1] = '\0';
        }
    } else {
        name[0] = '\0';
    }
    /* MAC: lowercase colon-separated hex, "" when length is 0. */
    if (a->PhysicalAddressLength > 0 &&
            a->PhysicalAddressLength <= (ULONG)sizeof(a->PhysicalAddress)) {
        size_t pos = 0;
        ULONG k;
        for (k = 0; k < a->PhysicalAddressLength; k++) {
            int n;
            if (k == 0) {
                n = snprintf(mac + pos, sizeof(mac) - pos, "%02x",
                    (unsigned)a->PhysicalAddress[k]);
            } else {
                n = snprintf(mac + pos, sizeof(mac) - pos, ":%02x",
                    (unsigned)a->PhysicalAddress[k]);
            }
            if (n < 0 || (size_t)n >= sizeof(mac) - pos) {
                mac[0] = '\0';
                break;
            }
            pos += (size_t)n;
        }
    } else {
        mac[0] = '\0';
    }
    /* First unicast IPv4 / IPv6 (scope suffix stripped). */
    if (WSAStartup(MAKEWORD(2, 2), &wsa) == 0) {
        int have4 = 0;
        int have6 = 0;
        for (ua = a->FirstUnicastAddress;
                ua != 0 && (!have4 || !have6);
                ua = ua->Next) {
            int family;
            if (ua->Address.lpSockaddr == 0) {
                continue;
            }
            if (ua->Address.iSockaddrLength <= 0) {
                continue;
            }
            family = ua->Address.lpSockaddr->sa_family;
            if (family == AF_INET && !have4) {
                char tmp[64];
                DWORD len = (DWORD)sizeof(tmp);
                if (WSAAddressToStringA(ua->Address.lpSockaddr,
                        (DWORD)ua->Address.iSockaddrLength,
                        0, tmp, &len) == 0) {
                    tmp[sizeof(tmp) - 1] = '\0';
                    strncpy(ipv4, tmp, sizeof(ipv4) - 1);
                    ipv4[sizeof(ipv4) - 1] = '\0';
                    have4 = 1;
                }
            } else if (family == AF_INET6 && !have6) {
                char tmp[64];
                DWORD len = (DWORD)sizeof(tmp);
                if (WSAAddressToStringA(ua->Address.lpSockaddr,
                        (DWORD)ua->Address.iSockaddrLength,
                        0, tmp, &len) == 0) {
                    char *pct;
                    tmp[sizeof(tmp) - 1] = '\0';
                    pct = strchr(tmp, '%');
                    if (pct != 0) {
                        *pct = '\0';
                    }
                    strncpy(ipv6, tmp, sizeof(ipv6) - 1);
                    ipv6[sizeof(ipv6) - 1] = '\0';
                    have6 = 1;
                }
            }
        }
        WSACleanup();
    }
    up = (a->OperStatus == IfOperStatusUp) ? "1" : "0";
    loop = (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) ? "1" : "0";
    snprintf(g_net, sizeof(g_net), "%s|%s|%s|%s|%s|%s",
        name, mac, ipv4, ipv6, up, loop);
    free(list);
    return g_net;
}

static char g_net_counters[16384];

const char *sysinfo_net_counters_all(void) {
    IP_ADAPTER_ADDRESSES *list;
    IP_ADAPTER_ADDRESSES *a;
    PMIB_IF_TABLE2 tbl = 0;
    size_t pos = 0;
    g_net_counters[0] = '\0';
    list = win32_net_enum();
    if (!list) {
        return "";
    }
    if (GetIfTable2(&tbl) != NO_ERROR || tbl == 0) {
        free(list);
        return "";
    }
    for (a = list; a != 0; a = a->Next) {
        MIB_IF_ROW2 *row = 0;
        ULONG i;
        char name[256] = { 0 };
        char line[512];
        int n;
        size_t need;
        size_t k;
        if (tbl == 0) {
            break;
        }
        for (i = 0; i < tbl->NumEntries; i++) {
            if (tbl->Table[i].InterfaceIndex == a->IfIndex) {
                row = &tbl->Table[i];
                break;
            }
        }
        if (!row) {
            continue;
        }
        if (a->FriendlyName != 0) {
            if (WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1,
                    name, (int)sizeof(name), 0, 0) == 0) {
                name[0] = '\0';
            } else {
                name[sizeof(name) - 1] = '\0';
            }
        } else {
            name[0] = '\0';
        }
        n = snprintf(line, sizeof(line), "%s|%llu|%llu",
            name, (unsigned long long)row->InOctets,
            (unsigned long long)row->OutOctets);
        if (n < 0) {
            continue;
        }
        if ((size_t)n >= sizeof(line)) {
            n = (int)sizeof(line) - 1;
            line[n] = '\0';
        }
        need = (size_t)n + (pos > 0 ? 1 : 0) + 1;
        if (pos + need > sizeof(g_net_counters)) {
            break;
        }
        if (pos > 0) {
            g_net_counters[pos++] = '\n';
        }
        for (k = 0; k < (size_t)n; k++) {
            g_net_counters[pos++] = line[k];
        }
        g_net_counters[pos] = '\0';
    }
    free(list);
    FreeMibTable(tbl);
    return g_net_counters;
}
