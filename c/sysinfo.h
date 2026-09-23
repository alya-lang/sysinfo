#ifndef ALYA_SYSINFO_H
#define ALYA_SYSINFO_H

int alya_sysinfo_add(int a, int b);

/* System locale tag (e.g. "tr-TR", "en-US").
 * Implemented per-OS in c/win32_locale.c, c/cocoa_locale.c, c/linux_locale.c.
 * Returns "" when unknown; Alya side falls back to env vars. */
const char *sysinfo_native_locale(void);

/* System information natives.
 * Implemented per-OS in c/win32_sysinfo.c, c/cocoa_sysinfo.c, c/linux_sysinfo.c.
 * Numeric functions return -1 when unavailable; string functions return "". */
int sysinfo_cpu_cores(void);
long long sysinfo_mem_total(void);
long long sysinfo_mem_avail(void);
const char *sysinfo_os_version(void);
const char *sysinfo_cpu_model(void);
const char *sysinfo_host_name(void);
long long sysinfo_uptime_sec(void);
long long sysinfo_disk_total(const char *path);
long long sysinfo_disk_free(const char *path);
const char *sysinfo_tz_name(void);
int sysinfo_utc_offset_min(void);

#endif
