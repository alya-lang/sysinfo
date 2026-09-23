#include "sysinfo.h"
#include <windows.h>

static char g_buf[128];

const char *sysinfo_native_locale(void) {
    wchar_t wbuf[128];
    if (GetUserDefaultLocaleName(wbuf, 128) == 0) {
        return "";
    }
    /* Locale names are ASCII (e.g. L"tr-TR", L"en-US"), narrow directly. */
    for (int i = 0; i < 127; i++) {
        wchar_t wc = wbuf[i];
        g_buf[i] = (char)wc;
        if (wc == 0) {
            break;
        }
    }
    g_buf[127] = '\0';
    return g_buf;
}
