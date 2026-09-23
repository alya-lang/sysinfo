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

/* ---- Extended detail APIs ---- */

#include <sys/mount.h>
#include <sys/utsname.h>
#include <mach-o/dyld.h>
#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

static char g_kernel[128];

const char *sysinfo_kernel_version(void) {
    struct utsname u;
    memset(&u, 0, sizeof(u));
    if (uname(&u) != 0) {
        return "";
    }
    strncpy(g_kernel, u.release, sizeof(g_kernel) - 1);
    g_kernel[sizeof(g_kernel) - 1] = '\0';
    return g_kernel;
}

const char *sysinfo_distro_id(void) {
    return "";
}

const char *sysinfo_distro_version(void) {
    return "";
}

int sysinfo_cpu_physical(void) {
    int n = 0;
    size_t s = sizeof(n);
    if (sysctlbyname("hw.physicalcpu", &n, &s, 0, 0) != 0 || n < 1) {
        return -1;
    }
    return n;
}

long long sysinfo_cpu_freq_mhz(void) {
    uint64_t hz = 0;
    size_t s = sizeof(hz);
    if (sysctlbyname("hw.cpufrequency", &hz, &s, 0, 0) != 0 || hz == 0) {
        return -1;
    }
    return (long long)(hz / 1000000ULL);
}

static char g_vendor[128];

const char *sysinfo_cpu_vendor(void) {
    size_t s = sizeof(g_vendor);
    if (sysctlbyname("machdep.cpu.vendor", g_vendor, &s, 0, 0) == 0) {
        return g_vendor;
    }
    return "Apple";
}

long long sysinfo_swap_total(void) {
    struct xsw_usage sw;
    size_t s = sizeof(sw);
    memset(&sw, 0, sizeof(sw));
    if (sysctlbyname("vm.swapusage", &sw, &s, 0, 0) != 0) {
        return -1;
    }
    return (long long)sw.xsu_total;
}

long long sysinfo_swap_avail(void) {
    struct xsw_usage sw;
    size_t s = sizeof(sw);
    memset(&sw, 0, sizeof(sw));
    if (sysctlbyname("vm.swapusage", &sw, &s, 0, 0) != 0) {
        return -1;
    }
    return (long long)sw.xsu_avail;
}

static int iokit_battery(int *pct, int *ac) {
    CFTypeRef blob = IOPSCopyPowerSourcesInfo();
    if (!blob) {
        return 0;
    }
    CFArrayRef list = IOPSCopyPowerSourcesList(blob);
    int ok = 0;
    if (list) {
        CFIndex n = CFArrayGetCount(list);
        for (CFIndex i = 0; i < n; i++) {
            CFDictionaryRef d = IOPSGetPowerSourceDescription(
                blob, CFArrayGetValueAtIndex(list, i));
            if (!d) {
                continue;
            }
            CFStringRef state = CFDictionaryGetValue(d, CFSTR(kIOPSPowerSourceStateKey));
            int powered = state && CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo;
            CFStringRef transport = CFDictionaryGetValue(d, CFSTR(kIOPSTransportTypeKey));
            int is_internal = transport && CFStringCompare(transport, CFSTR(kIOPSInternalType), 0) == kCFCompareEqualTo;
            if (is_internal) {
                CFNumberRef cur = CFDictionaryGetValue(d, CFSTR(kIOPSCurrentCapacityKey));
                CFNumberRef max = CFDictionaryGetValue(d, CFSTR(kIOPSMaxCapacityKey));
                int c = 0, m = 0;
                if (cur && max && CFNumberGetValue(cur, kCFNumberIntType, &c)
                        && CFNumberGetValue(max, kCFNumberIntType, &m) && m > 0) {
                    if (pct) {
                        *pct = (c * 100) / m;
                    }
                    ok = 1;
                }
            }
            if (ac && (*ac < 0 || powered)) {
                *ac = powered ? 1 : 0;
                ok = 1;
            }
        }
        CFRelease(list);
    }
    CFRelease(blob);
    return ok;
}

int sysinfo_battery_percent(void) {
    int pct = -1, ac = -1;
    if (iokit_battery(&pct, &ac) && pct >= 0 && pct <= 100) {
        return pct;
    }
    return -1;
}

int sysinfo_on_ac(void) {
    int pct = -1, ac = -1;
    if (iokit_battery(&pct, &ac) && ac >= 0) {
        return ac;
    }
    return -1;
}

static char g_exe[2048];

const char *sysinfo_exe_path(void) {
    uint32_t s = sizeof(g_exe);
    if (_NSGetExecutablePath(g_exe, &s) != 0) {
        return "";
    }
    return g_exe;
}

extern char **environ;

static char g_env[65536];

const char *sysinfo_env_block(void) {
    if (environ == 0) {
        return "";
    }
    size_t pos = 0;
    for (char **e = environ; *e != 0 && pos + 1 < sizeof(g_env); e++) {
        size_t n = strlen(*e);
        if (pos + n + 1 >= sizeof(g_env)) {
            break;
        }
        memcpy(g_env + pos, *e, n);
        pos += n;
        g_env[pos++] = '\n';
    }
    g_env[pos] = '\0';
    return g_env;
}

long long sysinfo_boot_unix(void) {
    int mib[2] = { CTL_KERN, KERN_BOOTTIME };
    struct timeval bt;
    size_t s = sizeof(bt);
    if (sysctl(mib, 2, &bt, &s, 0, 0) != 0) {
        return -1;
    }
    return (long long)bt.tv_sec;
}

static double load_at(int idx) {
    double avg[3] = { 0, 0, 0 };
    if (getloadavg(avg, 3) != 3) {
        return -1.0;
    }
    if (idx < 0 || idx > 2) {
        return -1.0;
    }
    return avg[idx];
}

double sysinfo_load_1(void) {
    return load_at(0);
}

double sysinfo_load_5(void) {
    return load_at(1);
}

double sysinfo_load_15(void) {
    return load_at(2);
}

int sysinfo_mount_count(void) {
    struct statfs *list = 0;
    int n = getmntinfo(&list, MNT_NOWAIT);
    return n < 0 ? 0 : n;
}

static char g_mount[2048];

const char *sysinfo_mount_at(int index) {
    struct statfs *list = 0;
    int n = getmntinfo(&list, MNT_NOWAIT);
    if (n <= 0 || index < 0 || index >= n) {
        return "";
    }
    snprintf(g_mount, sizeof(g_mount), "%s|%s",
        list[index].f_mntonname, list[index].f_fstypename);
    return g_mount;
}

static char g_fs[128];

const char *sysinfo_fs_type(const char *path) {
    struct statfs s;
    const char *p = (path && path[0]) ? path : "/";
    memset(&s, 0, sizeof(s));
    if (statfs(p, &s) != 0) {
        return "";
    }
    strncpy(g_fs, s.f_fstypename, sizeof(g_fs) - 1);
    g_fs[sizeof(g_fs) - 1] = '\0';
    return g_fs;
}

int sysinfo_is_elevated(void) {
    return geteuid() == 0 ? 1 : 0;
}
