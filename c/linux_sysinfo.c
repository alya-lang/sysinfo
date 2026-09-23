#define _POSIX_C_SOURCE 200809L
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
