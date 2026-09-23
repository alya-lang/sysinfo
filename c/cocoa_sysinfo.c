#include "sysinfo.h"
#include <sys/sysctl.h>
#include <sys/statvfs.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <CoreFoundation/CoreFoundation.h>

int sysinfo_cpu_cores(void) {
    int n = 0;
    size_t s = sizeof(n);
    if (sysctlbyname("hw.logicalcpu", &n, &s, 0, 0) != 0 || n < 1) {
        return 1;
    }
    return n;
}

long long sysinfo_mem_total(void) {
    uint64_t m = 0;
    size_t s = sizeof(m);
    if (sysctlbyname("hw.memsize", &m, &s, 0, 0) != 0) {
        return -1;
    }
    return (long long)m;
}

long long sysinfo_mem_avail(void) {
    vm_size_t page = 0;
    host_page_size(mach_host_self(), &page);
    vm_statistics64_data_t vm;
    mach_msg_type_number_t cnt = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
            (host_info64_t)&vm, &cnt) != KERN_SUCCESS) {
        return -1;
    }
    return (long long)(vm.free_count + vm.inactive_count) * (long long)page;
}

static char g_ver[128];

const char *sysinfo_os_version(void) {
    char build[64] = { 0 };
    size_t s = sizeof(build);
    if (sysctlbyname("kern.osproductversion", build, &s, 0, 0) != 0) {
        return "macOS";
    }
    snprintf(g_ver, sizeof(g_ver), "macOS %s", build);
    return g_ver;
}

static char g_cpu[256];

const char *sysinfo_cpu_model(void) {
    size_t s = sizeof(g_cpu);
    if (sysctlbyname("machdep.cpu.brand_string", g_cpu, &s, 0, 0) == 0) {
        return g_cpu;
    }
    s = sizeof(g_cpu);
    if (sysctlbyname("hw.model", g_cpu, &s, 0, 0) == 0) {
        return g_cpu;
    }
    return "";
}

static char g_host[256];

const char *sysinfo_host_name(void) {
    if (gethostname(g_host, sizeof(g_host)) != 0) {
        return "";
    }
    return g_host;
}

long long sysinfo_uptime_sec(void) {
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    struct timeval bt;
    size_t s = sizeof(bt);
    if (sysctl(mib, 2, &bt, &s, 0, 0) != 0) {
        return -1;
    }
    return (long long)(time(0) - bt.tv_sec);
}

long long sysinfo_disk_total(const char *path) {
    struct statvfs v;
    const char *p = (path != 0 && path[0] != '\0') ? path : "/";
    if (statvfs(p, &v) != 0) {
        return -1;
    }
    return (long long)v.f_blocks * (long long)v.f_frsize;
}

long long sysinfo_disk_free(const char *path) {
    struct statvfs v;
    const char *p = (path != 0 && path[0] != '\0') ? path : "/";
    if (statvfs(p, &v) != 0) {
        return -1;
    }
    return (long long)v.f_bavail * (long long)v.f_frsize;
}

static char g_tz[128];

const char *sysinfo_tz_name(void) {
    CFTimeZoneRef tz = CFTimeZoneCopySystem();
    if (tz == 0) {
        return "";
    }
    CFStringRef n = CFTimeZoneGetName(tz);
    int ok = 0;
    if (n != 0) {
        ok = CFStringGetCString(n, g_tz, sizeof(g_tz), kCFStringEncodingUTF8);
    }
    CFRelease(tz);
    if (!ok) {
        return "";
    }
    return g_tz;
}

int sysinfo_utc_offset_min(void) {
    CFTimeZoneRef tz = CFTimeZoneCopySystem();
    if (tz == 0) {
        return 0;
    }
    double secs = CFTimeZoneGetSecondsFromGMT(tz, CFAbsoluteTimeGetCurrent());
    CFRelease(tz);
    return (int)(secs / 60.0);
}
