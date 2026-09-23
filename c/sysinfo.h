#ifndef ALYA_SYSINFO_H
#define ALYA_SYSINFO_H

int alya_sysinfo_add(int a, int b);

/* Returns BCP-47-ish system locale tag (e.g. "tr-TR", "en-US").
 * Implemented per-OS in c/win32_locale.c, c/cocoa_locale.c, c/linux_locale.c.
 * Returns "" when unknown; Alya side falls back to env vars. */
const char *sysinfo_native_locale(void);

#endif
