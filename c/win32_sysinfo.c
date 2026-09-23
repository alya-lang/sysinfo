#include "sysinfo.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int sysinfo_cpu_cores(void) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    if (si.dwNumberOfProcessors < 1) {
        return 1;
    }
    return (int)si.dwNumberOfProcessors;
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
                snprintf(g_ver, sizeof(g_ver), "%s (%lu.%lu.%lu)",
                    label, vi.major, vi.minor, vi.build);
                return g_ver;
            }
        }
    }
    return "Windows";
}

static char g_cpu[256];

const char *sysinfo_cpu_model(void) {
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
    DYNAMIC_TIME_ZONE_INFORMATION tz;
    memset(&tz, 0, sizeof(tz));
    if (GetDynamicTimeZoneInformation(&tz) == TIME_ZONE_ID_INVALID) {
        return 0;
    }
    return -(tz.Bias);
}

/* ---- Extended detail APIs ---- */

#include <cpuid.h>

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

const char *sysinfo_distro_id(void) {
    return "";
}

const char *sysinfo_distro_version(void) {
    return "";
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

static char g_vendor[16];

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
    const char *arch = getenv("PROCESSOR_ARCHITECTURE");
    if (arch && arch[0]) {
        strncpy(g_vendor, arch, sizeof(g_vendor) - 1);
        g_vendor[sizeof(g_vendor) - 1] = '\0';
        return g_vendor;
    }
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
    return avail < 0 ? 0 : avail;
}

int sysinfo_battery_percent(void) {
    SYSTEM_POWER_STATUS sps;
    memset(&sps, 0, sizeof(sps));
    if (!GetSystemPowerStatus(&sps)) {
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
    DWORD n = GetModuleFileNameA(0, g_exe, sizeof(g_exe));
    if (n == 0 || n >= sizeof(g_exe)) {
        return "";
    }
    return g_exe;
}

static char g_env[65536];

const char *sysinfo_env_block(void) {
    char *block = GetEnvironmentStringsA();
    if (block == 0) {
        return "";
    }
    size_t pos = 0;
    const char *p = block;
    while (*p != '\0' && pos + 1 < sizeof(g_env)) {
        size_t n = strlen(p);
        if (pos + n + 1 >= sizeof(g_env)) {
            break;
        }
        memcpy(g_env + pos, p, n);
        pos += n;
        g_env[pos++] = '\n';
        p += n + 1;
    }
    g_env[pos] = '\0';
    FreeEnvironmentStringsA(block);
    return g_env;
}

long long sysinfo_boot_unix(void) {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER t;
    t.LowPart = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    long long unix = (long long)(t.QuadPart / 10000000ULL) - 11644473600LL;
    return unix - sysinfo_uptime_sec();
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
    GetVolumeInformationA(found, 0, 0, 0, 0, 0, fs, sizeof(fs));
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
        return 0;
    }
    BOOL member = FALSE;
    CheckTokenMembership(0, admin, &member);
    FreeSid(admin);
    return member ? 1 : 0;
}
