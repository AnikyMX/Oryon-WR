#include "state.h"
#include "../driver/driver.h"
#include "extensions.inc"

namespace oryon {

State g_state;

/* Versi yang dilaporkan menentukan jalur render yang dipilih Minecraft.
   Lihat docs/ARCHITECTURE.md bagian 4 sebelum mengubah angka ini. */
/* Naikkan tiap rilis: string ini muncul di log Minecraft, jadi bisa dipakai
   memastikan build mana yang sedang berjalan di perangkat. */
#define ORYON_BUILD "0.6.3"

static const char kVersion[]  = "2.1 Oryon";
static const char kGlsl[]     = "1.20";
static const char kVendor[]   = "Oryon";
static const char kExtensions[] = ORYON_EXTENSIONS;

/* Nama renderer diambil dari driver sungguhan supaya laporan bug tetap berguna. */
static char s_renderer[128];

void state_init() {
    g_state.error      = GL_NO_ERROR;
    g_state.version    = kVersion;
    g_state.glsl       = kGlsl;
    g_state.vendor     = kVendor;
    g_state.extensions = kExtensions;

    const char *real = 0;
    if (gles.glGetString) real = (const char *) gles.glGetString(GL_RENDERER);

    char *w = s_renderer;
    char *end = s_renderer + sizeof(s_renderer) - 1;
    for (const char *p = "Oryon " ORYON_BUILD " on "; *p && w < end; ++p) *w++ = *p;
    if (real) {
        for (const char *p = real; *p && w < end; ++p) *w++ = *p;
    } else {
        for (const char *p = "unknown GLES"; *p && w < end; ++p) *w++ = *p;
    }
    *w = 0;
    g_state.renderer = s_renderer;
}

}  /* namespace oryon */
