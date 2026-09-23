#define _GNU_SOURCE
#include "sysinfo.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/statvfs.h>
#include <time.h>

int sysinfo_cpu_cores(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 1) {
        return 1;
    }
    return (int)n;
}

static long long meminfo_kb(const char *key) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (f == 0) {
        return -1;
    }
    char line[256];
    size_t klen = strlen(key);
    long long out = -1;
    while (fgets(line, sizeof(line), f) != 0) {
        if (strncmp(line, key, klen) == 0) {
            long long v = -1;
            if (sscanf(line + klen, " %lld", &v) == 1 && v >= 0) {
                out = v * 1024;
            }
            break;
        }
    }
    fclose(f);
    return out;
}

long long sysinfo_mem_total(void) {
    return meminfo_kb("MemTotal:");
}

long long sysinfo_mem_avail(void) {
    long long a = meminfo_kb("MemAvailable:");
    if (a >= 0) {
        return a;
    }
    long long f = meminfo_kb("MemFree:");
    long long b = meminfo_kb("Buffers:");
    long long c = meminfo_kb("Cached:");
    if (f < 0) {
        return -1;
    }
    if (b < 0) {
        b = 0;
    }
    if (c < 0) {
        c = 0;
    }
    return f + b + c;
}

static char g_ver[256];

const char *sysinfo_os_version(void) {
    FILE *f = fopen("/etc/os-release", "r");
    if (f == 0) {
        return "Linux";
    }
    char line[512];
    g_ver[0] = '\0';
    while (fgets(line, sizeof(line), f) != 0) {
        if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
            char *v = line + 12;
            while (*v == ' ' || *v == '\t') {
                v++;
            }
            size_t n = strlen(v);
            while (n > 0 && (v[n - 1] == '\n' || v[n - 1] == '\r' || v[n - 1] == ' ')) {
                v[--n] = '\0';
            }
            if (n >= 2 && v[0] == '"' && v[n - 1] == '"') {
                v[n - 1] = '\0';
                v++;
            }
            strncpy(g_ver, v, sizeof(g_ver) - 1);
            g_ver[sizeof(g_ver) - 1] = '\0';
            break;
        }
    }
    fclose(f);
    if (g_ver[0] == '\0') {
        return "Linux";
    }
    return g_ver;
}

static char g_cpu[256];

const char *sysinfo_cpu_model(void) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f == 0) {
        return "";
    }
    char line[512];
    while (fgets(line, sizeof(line), f) != 0) {
        if (strncmp(line, "model name", 10) == 0) {
            char *c = strchr(line, ':');
            if (c != 0) {
                c++;
                while (*c == ' ' || *c == '\t') {
                    c++;
                }
                size_t n = strlen(c);
                while (n > 0 && (c[n - 1] == '\n' || c[n - 1] == '\r')) {
                    c[--n] = '\0';
                }
                strncpy(g_cpu, c, sizeof(g_cpu) - 1);
                g_cpu[sizeof(g_cpu) - 1] = '\0';
                fclose(f);
                return g_cpu;
            }
        }
    }
    fclose(f);
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
    FILE *f = fopen("/proc/uptime", "r");
    if (f == 0) {
        return -1;
    }
    double up = 0;
    int ok = fscanf(f, "%lf", &up);
    fclose(f);
    if (ok != 1 || up < 0) {
        return -1;
    }
    return (long long)up;
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

static char g_tz[256];

const char *sysinfo_tz_name(void) {
    char link[512];
    ssize_t n = readlink("/etc/localtime", link, sizeof(link) - 1);
    if (n > 0) {
        link[n] = '\0';
        const char *p = strstr(link, "zoneinfo/");
        if (p != 0) {
            strncpy(g_tz, p + 9, sizeof(g_tz) - 1);
            g_tz[sizeof(g_tz) - 1] = '\0';
            return g_tz;
        }
        strncpy(g_tz, link, sizeof(g_tz) - 1);
        g_tz[sizeof(g_tz) - 1] = '\0';
        return g_tz;
    }
    const char *t = getenv("TZ");
    if (t != 0 && t[0] != '\0') {
        strncpy(g_tz, t, sizeof(g_tz) - 1);
        g_tz[sizeof(g_tz) - 1] = '\0';
        return g_tz;
    }
    return "";
}

int sysinfo_utc_offset_min(void) {
    time_t t = time(0);
    struct tm lt;
    localtime_r(&t, &lt);
    return (int)(lt.tm_gmtoff / 60);
}

/* ---- Extended detail APIs ---- */

#include <sys/utsname.h>
#include <sys/vfs.h>

static char g_kernel[256];

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

static int os_release_value(const char *key, char *out, size_t outsz) {
    FILE *f = fopen("/etc/os-release", "r");
    if (!f || !key || !out || outsz == 0) {
        return 0;
    }
    char line[512];
    size_t klen = strlen(key);
    int found = 0;
    while (fgets(line, sizeof(line), f) != 0) {
        if (strncmp(line, key, klen) == 0 && line[klen] == '=') {
            char *v = line + klen + 1;
            while (*v == ' ' || *v == '\t') {
                v++;
            }
            size_t n = strlen(v);
            while (n > 0 && (v[n - 1] == '\n' || v[n - 1] == '\r' || v[n - 1] == ' ')) {
                v[--n] = '\0';
            }
            if (n >= 2 && v[0] == '"' && v[n - 1] == '"') {
                v[n - 1] = '\0';
                v++;
            }
            strncpy(out, v, outsz - 1);
            out[outsz - 1] = '\0';
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

static char g_distro_id[128];

const char *sysinfo_distro_id(void) {
    g_distro_id[0] = '\0';
    if (!os_release_value("ID", g_distro_id, sizeof(g_distro_id))) {
        return "";
    }
    return g_distro_id;
}

static char g_distro_ver[128];

const char *sysinfo_distro_version(void) {
    g_distro_ver[0] = '\0';
    if (!os_release_value("VERSION_ID", g_distro_ver, sizeof(g_distro_ver))) {
        return "";
    }
    return g_distro_ver;
}

static long cpuinfo_long(const char *key, long fallback) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        return fallback;
    }
    char line[512];
    size_t klen = strlen(key);
    long out = fallback;
    while (fgets(line, sizeof(line), f) != 0) {
        if (strncmp(line, key, klen) == 0) {
            char *c = strchr(line, ':');
            if (c) {
                long v = -1;
                if (sscanf(c + 1, " %ld", &v) == 1 && v > 0) {
                    out = v;
                    break;
                }
            }
        }
    }
    fclose(f);
    return out;
}

int sysinfo_cpu_physical(void) {
    long per_pkg = cpuinfo_long("cpu cores", -1);
    if (per_pkg < 1) {
        return sysinfo_cpu_cores();
    }
    /* Count physical packages. */
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        return (int)per_pkg;
    }
    char line[512];
    int pkgs[16];
    int npkg = 0;
    while (fgets(line, sizeof(line), f) != 0) {
        if (strncmp(line, "physical id", 11) == 0) {
            char *c = strchr(line, ':');
            if (c) {
                int id = -1;
                if (sscanf(c + 1, " %d", &id) == 1 && id >= 0) {
                    int seen = 0;
                    for (int i = 0; i < npkg; i++) {
                        if (pkgs[i] == id) {
                            seen = 1;
                            break;
                        }
                    }
                    if (!seen && npkg < 16) {
                        pkgs[npkg++] = id;
                    }
                }
            }
        }
    }
    fclose(f);
    if (npkg < 1) {
        return (int)per_pkg;
    }
    return npkg * (int)per_pkg;
}

long long sysinfo_cpu_freq_mhz(void) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        return -1;
    }
    char line[512];
    while (fgets(line, sizeof(line), f) != 0) {
        if (strncmp(line, "cpu MHz", 7) == 0) {
            char *c = strchr(line, ':');
            if (c) {
                double mhz = 0;
                if (sscanf(c + 1, " %lf", &mhz) == 1 && mhz > 0) {
                    fclose(f);
                    return (long long)mhz;
                }
            }
        }
    }
    fclose(f);
    return -1;
}

static char g_vendor[128];

const char *sysinfo_cpu_vendor(void) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        return "";
    }
    char line[512];
    while (fgets(line, sizeof(line), f) != 0) {
        if (strncmp(line, "vendor_id", 9) == 0) {
            char *c = strchr(line, ':');
            if (c) {
                c++;
                while (*c == ' ' || *c == '\t') {
                    c++;
                }
                size_t n = strlen(c);
                while (n > 0 && (c[n - 1] == '\n' || c[n - 1] == '\r')) {
                    c[--n] = '\0';
                }
                strncpy(g_vendor, c, sizeof(g_vendor) - 1);
                g_vendor[sizeof(g_vendor) - 1] = '\0';
                fclose(f);
                return g_vendor;
            }
        }
    }
    fclose(f);
    return "";
}

long long sysinfo_swap_total(void) {
    return meminfo_kb("SwapTotal:");
}

long long sysinfo_swap_avail(void) {
    return meminfo_kb("SwapFree:");
}

static int read_long_file(const char *path, long *out) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return 0;
    }
    long v = 0;
    int ok = fscanf(f, "%ld", &v) == 1;
    fclose(f);
    if (ok && out) {
        *out = v;
    }
    return ok;
}

int sysinfo_battery_percent(void) {
    const char *bats[] = { "BAT0", "BAT1", "BAT2" };
    char p[160];
    for (int i = 0; i < 3; i++) {
        snprintf(p, sizeof(p), "/sys/class/power_supply/%s/capacity", bats[i]);
        long v = -1;
        if (read_long_file(p, &v) && v >= 0 && v <= 100) {
            return (int)v;
        }
    }
    return -1;
}

int sysinfo_on_ac(void) {
    const char *acs[] = { "AC", "ACAD", "ADP0", "ADP1" };
    char p[160];
    for (int i = 0; i < 4; i++) {
        snprintf(p, sizeof(p), "/sys/class/power_supply/%s/online", acs[i]);
        long v = -1;
        if (read_long_file(p, &v)) {
            return v ? 1 : 0;
        }
    }
    return -1;
}

static char g_exe[2048];

const char *sysinfo_exe_path(void) {
    ssize_t n = readlink("/proc/self/exe", g_exe, sizeof(g_exe) - 1);
    if (n <= 0) {
        return "";
    }
    g_exe[n] = '\0';
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
    FILE *f = fopen("/proc/stat", "r");
    if (!f) {
        return -1;
    }
    char line[256];
    long long btime = -1;
    while (fgets(line, sizeof(line), f) != 0) {
        if (strncmp(line, "btime ", 6) == 0) {
            long long v = -1;
            if (sscanf(line + 6, " %lld", &v) == 1 && v > 0) {
                btime = v;
            }
            break;
        }
    }
    fclose(f);
    return btime;
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
    FILE *f = fopen("/proc/mounts", "r");
    if (!f) {
        return 0;
    }
    char line[1024];
    int n = 0;
    while (fgets(line, sizeof(line), f) != 0 && n < 256) {
        n++;
    }
    fclose(f);
    return n;
}

static char g_mount[1024];

const char *sysinfo_mount_at(int index) {
    FILE *f = fopen("/proc/mounts", "r");
    if (!f || index < 0) {
        return "";
    }
    char line[1024];
    int i = 0;
    while (fgets(line, sizeof(line), f) != 0) {
        if (i == index) {
            char dev[512], mnt[512], fs[128];
            if (sscanf(line, "%511s %511s %127s", dev, mnt, fs) == 3) {
                snprintf(g_mount, sizeof(g_mount), "%s|%s", mnt, fs);
                fclose(f);
                return g_mount;
            }
            break;
        }
        i++;
    }
    fclose(f);
    return "";
}

static const char *fs_name(long t) {
    switch ((unsigned long)t) {
        case 0xEF53: return "ext4";
        case 0x01021994: return "tmpfs";
        case 0x9123683E: return "btrfs";
        case 0x58465342: return "xfs";
        case 0x6969: return "nfs";
        case 0xFF534D42: return "cifs";
        case 0x4D44: return "fat";
        case 0x794C7630: return "overlayfs";
        case 0x65735546: return "fuse";
        case 0x53464846: return "wslfs";
        case 0x9FA0: return "proc";
        case 0x1CD1: return "devpts";
        case 0x62656572: return "sysfs";
        case 0xEF51: return "ext2";
        default: return "";
    }
}

static char g_fs[64];

const char *sysinfo_fs_type(const char *path) {
    struct statfs s;
    const char *p = (path && path[0]) ? path : "/";
    memset(&s, 0, sizeof(s));
    if (statfs(p, &s) != 0) {
        return "";
    }
    const char *name = fs_name(s.f_type);
    strncpy(g_fs, name, sizeof(g_fs) - 1);
    g_fs[sizeof(g_fs) - 1] = '\0';
    return g_fs;
}

int sysinfo_is_elevated(void) {
    return geteuid() == 0 ? 1 : 0;
}
