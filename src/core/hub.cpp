/* Hub resolusi simbol: satu-satunya pintu masuk LWJGL.

   Urutan pencarian LWJGL (diverifikasi dari bytecode org/lwjgl/opengl/GL$1):
     glXGetProcAddress -> glXGetProcAddressARB -> wglGetProcAddress
       -> eglGetProcAddress -> OSMesaGetProcAddress -> fallback dlsym()

   Aturan mati: nama yang tidak dikenal WAJIB mengembalikan NULL. LWJGL memakai
   NULL untuk menyimpulkan flag kapabilitas bernilai false. Mengembalikan
   penunjuk asal-asalan akan membuat Checks.check() melempar di panggilan
   pertama, jauh dari penyebabnya. */

#include "oryon/oryon.h"
#include "../driver/driver.h"

#include <string.h>

/* Deklarasi seluruh 352 ekspor, dibangkitkan dari ground-truth. */
#define ORYON_SYM(strat, ret, name, params, args) ORYON_API ret name params;
#include "oryon/gl_symbols.inc"

namespace {

struct Entry {
    const char *name;
    void *fn;
};

#define ORYON_HUB_BEGIN(n)
#define ORYON_HUB(n, f) { n, (void *) f },
#define ORYON_HUB_END()
const Entry kTable[] = {
#include "hub_table.inc"
};
#undef ORYON_HUB_BEGIN
#undef ORYON_HUB
#undef ORYON_HUB_END

const unsigned kCount = (unsigned) (sizeof(kTable) / sizeof(kTable[0]));

/* Tabel terurut strcmp saat dibangkitkan; pencarian biner sah. Sekitar 9
   perbandingan, dan hanya terjadi saat inisialisasi LWJGL. */
void *lookup(const char *name) {
    if (ORYON_UNLIKELY(!name)) return 0;
    unsigned lo = 0, hi = kCount;
    while (lo < hi) {
        unsigned mid = (lo + hi) >> 1;
        int c = strcmp(kTable[mid].name, name);
        if (c < 0)       lo = mid + 1;
        else if (c > 0)  hi = mid;
        else             return kTable[mid].fn;
    }
    return 0;   /* tidak dikenal -> NULL, tanpa kecuali */
}

}  /* namespace */

ORYON_API void *glXGetProcAddress(const GLubyte *name) {
    ::oryon::ensure_init();
    return lookup((const char *) name);
}

ORYON_API void *glXGetProcAddressARB(const GLubyte *name) {
    ::oryon::ensure_init();
    return lookup((const char *) name);
}

ORYON_API void *OSMesaGetProcAddress(const char *name) {
    ::oryon::ensure_init();
    return lookup(name);
}

/* dlopen memakai RTLD_GLOBAL, sehingga simbol ini bisa membayangi milik
   libpojavexec.so. Nama ber-awalan "egl" diteruskan ke libEGL sungguhan supaya
   perilaku tetap benar apa pun urutan pemuatan. */
ORYON_API void *eglGetProcAddress(const char *name) {
    if (ORYON_UNLIKELY(!name)) return 0;
    if (name[0] == 'e' && name[1] == 'g' && name[2] == 'l')
        return ::oryon::egl_get_proc(name);
    ::oryon::ensure_init();
    return lookup(name);
}
