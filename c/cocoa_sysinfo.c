#include "sysinfo.h"
#include <sys/sysctl.h>
#include <sys/statvfs.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <time.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
    long long pages = (long long)vm.free_count + (long long)vm.inactive_count;
#ifndef SYSINFO_NO_SPECULATIVE
    /* speculative_count has shipped in vm_statistics64 for many SDK
     * generations; there is no feature macro for a struct member, so
     * build older SDKs with -DSYSINFO_NO_SPECULATIVE to skip it. */
    pages += (long long)vm.speculative_count;
#endif
    return pages * (long long)page;
}

int sysinfo_mem_estimated(void) {
    return 0;
}

static char g_ver[128];

const char *sysinfo_os_version(void) {
    char build[64] = { 0 };
    size_t s = sizeof(build);
    if (sysctlbyname("kern.osproductversion", build, &s, 0, 0) != 0) {
        return "macOS";
    }
    /* Attach the marketing name for known major releases. */
    const char *marketing = "";
    int major = atoi(build);
    if (major >= 26) {
        marketing = "Tahoe ";
    } else if (major == 15) {
        marketing = "Sequoia ";
    } else if (major == 14) {
        marketing = "Sonoma ";
    } else if (major == 13) {
        marketing = "Ventura ";
    } else if (major == 12) {
        marketing = "Monterey ";
    } else if (major == 11) {
        marketing = "Big Sur ";
    }
    snprintf(g_ver, sizeof(g_ver), "macOS %s%s", marketing, build);
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

/* hw.cpufrequency is the current (Intel) clock; hw.cpufrequency_max is the
 * base/max clock. Apple Silicon omits hw.cpufrequency, so fall back to
 * hw.cpufrequency_max. Either way this is a base rating, not live turbo. */
long long sysinfo_cpu_freq_mhz(void) {
    uint64_t hz = 0;
    size_t s = sizeof(hz);
    if (sysctlbyname("hw.cpufrequency", &hz, &s, 0, 0) == 0 && hz != 0) {
        return (long long)(hz / 1000000ULL);
    }
    hz = 0;
    s = sizeof(hz);
    if (sysctlbyname("hw.cpufrequency_max", &hz, &s, 0, 0) == 0 && hz != 0) {
        return (long long)(hz / 1000000ULL);
    }
    return -1;
}

static char g_vendor[128];

const char *sysinfo_cpu_vendor(void) {
    size_t s = sizeof(g_vendor);
    if (sysctlbyname("machdep.cpu.vendor", g_vendor, &s, 0, 0) == 0) {
        return g_vendor;
    }
    /* Apple Silicon has no machdep.cpu.vendor; hw.model contains "Mac". */
    char model[256];
    s = sizeof(model);
    if (sysctlbyname("hw.model", model, &s, 0, 0) == 0) {
        if (strstr(model, "Mac") != 0) {
            return "Apple";
        }
    }
    return "";
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

static char g_exe_real[2048];

const char *sysinfo_exe_path(void) {
    uint32_t s = sizeof(g_exe);
    if (_NSGetExecutablePath(g_exe, &s) != 0) {
        return "";
    }
    if (realpath(g_exe, g_exe_real) != 0) {
        return g_exe_real;
    }
    return g_exe;
}

extern char **environ;

static char g_env[65536];

static int g_env_truncated = 0;

const char *sysinfo_env_block(void) {
    g_env_truncated = 0;
    if (environ == 0) {
        return "";
    }
    size_t pos = 0;
    for (char **e = environ; *e != 0 && pos + 1 < sizeof(g_env); e++) {
        size_t n = strlen(*e);
        if (pos + n + 1 >= sizeof(g_env)) {
            g_env_truncated = 1;
            break;
        }
        memcpy(g_env + pos, *e, n);
        pos += n;
        g_env[pos++] = '\n';
    }
    g_env[pos] = '\0';
    return g_env;
}

int sysinfo_env_truncated(void) {
    return g_env_truncated;
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
    int n = getloadavg(avg, 3);
    if (n < 1) {
        return -1.0;
    }
    if (idx < 0 || idx > 2) {
        return -1.0;
    }
    if (idx < n) {
        return avg[idx];
    }
    return -1.0;
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

const char *sysinfo_user_name(void) {
    struct passwd *pw = getpwuid(getuid());
    if (!pw || !pw->pw_name || pw->pw_name[0] == '\0') {
        return "";
    }
    return pw->pw_name;
}

static char g_host_native[256];

const char *sysinfo_host_native(void) {
    if (gethostname(g_host_native, sizeof(g_host_native)) != 0) {
        return "";
    }
    g_host_native[sizeof(g_host_native) - 1] = '\0';
    return g_host_native;
}

static char g_net[1024];

int sysinfo_net_count(void) {
    struct ifaddrs *head = 0;
    struct ifaddrs *p = 0;
    char seen[256][64];
    int n = 0;
    int i = 0;
    int found = 0;
    if (getifaddrs(&head) != 0) {
        return -1;
    }
    for (p = head; p != 0; p = p->ifa_next) {
        if (p->ifa_name == 0) {
            continue;
        }
        found = 0;
        for (i = 0; i < n; i++) {
            if (strcmp(seen[i], p->ifa_name) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            if (n >= 256) {
                break;
            }
            strncpy(seen[n], p->ifa_name, sizeof(seen[n]) - 1);
            seen[n][sizeof(seen[n]) - 1] = '\0';
            n++;
        }
    }
    freeifaddrs(head);
    return n;
}

const char *sysinfo_net_at(int index) {
    struct ifaddrs *head = 0;
    struct ifaddrs *p = 0;
    char target[128];
    char mac[32];
    char ipv4[64];
    char v6first[64];
    char v6pref[64];
    const char *v6 = 0;
    int up = 0;
    int lo = 0;
    int have_target = 0;
    if (index < 0) {
        return "";
    }
    if (getifaddrs(&head) != 0) {
        return "";
    }
    target[0] = '\0';
    {
        char seen[256][64];
        int n = 0;
        int k = 0;
        int found = 0;
        for (p = head; p != 0; p = p->ifa_next) {
            if (p->ifa_name == 0) {
                continue;
            }
            found = 0;
            for (k = 0; k < n; k++) {
                if (strcmp(seen[k], p->ifa_name) == 0) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                continue;
            }
            if (n < 256) {
                strncpy(seen[n], p->ifa_name, sizeof(seen[n]) - 1);
                seen[n][sizeof(seen[n]) - 1] = '\0';
                if (n == index) {
                    strncpy(target, p->ifa_name, sizeof(target) - 1);
                    target[sizeof(target) - 1] = '\0';
                    have_target = 1;
                }
                n++;
            }
        }
    }
    if (!have_target) {
        freeifaddrs(head);
        return "";
    }
    mac[0] = '\0';
    ipv4[0] = '\0';
    v6first[0] = '\0';
    v6pref[0] = '\0';
    for (p = head; p != 0; p = p->ifa_next) {
        if (p->ifa_name == 0) {
            continue;
        }
        if (strcmp(p->ifa_name, target) != 0) {
            continue;
        }
        if ((p->ifa_flags & IFF_UP) != 0) {
            up = 1;
        }
        if ((p->ifa_flags & IFF_LOOPBACK) != 0) {
            lo = 1;
        }
        if (p->ifa_addr == 0) {
            continue;
        }
        if (p->ifa_addr->sa_family == AF_LINK) {
            if (mac[0] == '\0') {
                struct sockaddr_dl *s = (struct sockaddr_dl *)p->ifa_addr;
                unsigned char *a = 0;
                unsigned int alen = (unsigned int)s->sdl_alen;
                unsigned int m = 0;
                unsigned int nonzero = 0;
                if (alen == 0) {
                    continue;
                }
                if (alen > 6) {
                    alen = 6;
                }
                a = (unsigned char *)LLADDR(s);
                for (m = 0; m < alen; m++) {
                    if (a[m] != 0) {
                        nonzero = 1;
                    }
                }
                if (!nonzero) {
                    continue; /* loopback/tunnel: no real MAC */
                }
                mac[0] = '\0';
                for (m = 0; m < alen; m++) {
                    char tmp[4];
                    snprintf(tmp, sizeof(tmp), "%02x", a[m]);
                    strcat(mac, tmp);
                    if (m + 1 < alen) {
                        strcat(mac, ":");
                    }
                }
            }
        } else if (p->ifa_addr->sa_family == AF_INET) {
            if (ipv4[0] == '\0') {
                char buf[64];
                struct sockaddr_in *s4 = (struct sockaddr_in *)p->ifa_addr;
                if (inet_ntop(AF_INET, &s4->sin_addr, buf, sizeof(buf)) != 0) {
                    strncpy(ipv4, buf, sizeof(ipv4) - 1);
                    ipv4[sizeof(ipv4) - 1] = '\0';
                }
            }
        } else if (p->ifa_addr->sa_family == AF_INET6) {
            char buf[128];
            char *pct = 0;
            struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)p->ifa_addr;
            int linklocal = 0;
            if (inet_ntop(AF_INET6, &s6->sin6_addr, buf, sizeof(buf)) == 0) {
                continue;
            }
            pct = strchr(buf, '%');
            if (pct != 0) {
                *pct = '\0';
            }
            if (buf[0] == '\0') {
                continue;
            }
            if (v6first[0] == '\0') {
                strncpy(v6first, buf, sizeof(v6first) - 1);
                v6first[sizeof(v6first) - 1] = '\0';
            }
            linklocal = (buf[0] == 'f' || buf[0] == 'F')
                && (buf[1] == 'e' || buf[1] == 'E')
                && buf[2] == '8' && buf[3] == '0' && buf[4] == ':';
            if (!linklocal && v6pref[0] == '\0') {
                strncpy(v6pref, buf, sizeof(v6pref) - 1);
                v6pref[sizeof(v6pref) - 1] = '\0';
            }
        }
    }
    freeifaddrs(head);
    v6 = v6pref[0] != '\0' ? v6pref : v6first;
    snprintf(g_net, sizeof(g_net), "%s|%s|%s|%s|%d|%d",
        target, mac, ipv4, v6, up, lo);
    return g_net;
}
