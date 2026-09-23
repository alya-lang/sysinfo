#include "sysinfo.h"
#include <locale.h>
#include <stdlib.h>

const char *sysinfo_native_locale(void) {
    /* Query only: a NULL second arg returns the current setting. */
    const char *s = setlocale(LC_MESSAGES, NULL);
    if (s != 0 && s[0] != '\0' && s[0] != 'C') {
        return s;
    }
    s = getenv("LC_ALL");
    if (s != 0 && s[0] != '\0') {
        return s;
    }
    s = getenv("LC_MESSAGES");
    if (s != 0 && s[0] != '\0') {
        return s;
    }
    s = getenv("LANG");
    if (s != 0 && s[0] != '\0') {
        return s;
    }
    return "";
}
