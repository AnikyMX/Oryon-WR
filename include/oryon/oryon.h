/* Header dalam bersama Oryon. */
#ifndef ORYON_ORYON_H
#define ORYON_ORYON_H

#include "oryon/gl.h"

/* Bionic tidak punya lazy binding: seluruh simbol diikat saat dlopen.
   Karena itu semua yang tidak diekspor harus benar-benar tersembunyi. */
#define ORYON_API    extern "C" __attribute__((visibility("default")))
#define ORYON_LOCAL  __attribute__((visibility("hidden")))
#define ORYON_INLINE inline __attribute__((always_inline))
#define ORYON_LIKELY(x)   __builtin_expect(!!(x), 1)
#define ORYON_UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace oryon {

#if defined(ORYON_DEBUG)
ORYON_LOCAL void log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#  define ORYON_LOG(...) ::oryon::log(__VA_ARGS__)
#else
#  define ORYON_LOG(...) ((void) 0)
#endif

/* Dipanggil sebelum perintah GL pertama apa pun. Sekali jalan, lalu gratis. */
ORYON_LOCAL bool ensure_init();

}  /* namespace oryon */

#endif /* ORYON_ORYON_H */
