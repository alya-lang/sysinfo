#ifndef ALYA_SYSINFO_H
#define ALYA_SYSINFO_H

int alya_sysinfo_add(int a, int b);

/* System locale tag (e.g. "tr-TR", "en-US").
 * Implemented per-OS in c/win32_locale.c, c/cocoa_locale.c, c/linux_locale.c.
 * Returns "" when unknown; Alya side falls back to env vars. */
const char *sysinfo_native_locale(void);

/* Core system information natives.
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

/* Extended detail natives (same per-OS files).
 * Same unknown conventions: -1 / -1.0 / "". */
const char *sysinfo_kernel_version(void);
const char *sysinfo_distro_id(void);
const char *sysinfo_distro_version(void);
int sysinfo_cpu_physical(void);
long long sysinfo_cpu_freq_mhz(void);
const char *sysinfo_cpu_vendor(void);
long long sysinfo_swap_total(void);
long long sysinfo_swap_avail(void);
int sysinfo_battery_percent(void);
int sysinfo_on_ac(void);
const char *sysinfo_exe_path(void);
const char *sysinfo_env_block(void);
long long sysinfo_boot_unix(void);
double sysinfo_load_1(void);
double sysinfo_load_5(void);
double sysinfo_load_15(void);
int sysinfo_mount_count(void);
const char *sysinfo_mount_at(int index);
const char *sysinfo_fs_type(const char *path);
int sysinfo_is_elevated(void);

#endif
