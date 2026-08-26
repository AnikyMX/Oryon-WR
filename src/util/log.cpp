#include "oryon/oryon.h"

#if defined(ORYON_DEBUG)
#include <stdarg.h>
#include <stdio.h>
#if defined(__ANDROID__)
#include <android/log.h>
#endif

namespace oryon {
void log(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
#if defined(__ANDROID__)
    __android_log_vprint(ANDROID_LOG_INFO, "Oryon", fmt, ap);
#else
    fputs("[Oryon] ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
#endif
    va_end(ap);
}
}  /* namespace oryon */
#endif
