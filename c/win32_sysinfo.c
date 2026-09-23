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
