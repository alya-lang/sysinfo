#include "sysinfo.h"
#include <CoreFoundation/CoreFoundation.h>

static char g_buf[128];

const char *sysinfo_native_locale(void) {
    CFLocaleRef loc = CFLocaleCopyCurrent();
    if (loc == 0) {
        return "";
    }
    CFStringRef ident = CFLocaleGetIdentifier(loc);
    if (ident == 0) {
        CFRelease(loc);
        return "";
    }
    int ok = CFStringGetCString(ident, g_buf, sizeof(g_buf), kCFStringEncodingUTF8);
    CFRelease(loc);
    if (!ok) {
        return "";
    }
    return g_buf;
}
