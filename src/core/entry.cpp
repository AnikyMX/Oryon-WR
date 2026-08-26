#include "oryon/oryon.h"
#include "../driver/driver.h"
#include "../state/state.h"
#include "../state/ff_state.h"

namespace oryon {

static bool s_ready;

/* Sengaja malas, bukan constructor .so: saat dlopen belum tentu ada context
   GLES yang current, dan state_init() membaca GL_RENDERER dari driver. */
bool ensure_init() {
    if (ORYON_LIKELY(s_ready)) return true;
    if (!driver_load()) return false;
    state_init();
    ff_init();
    s_ready = true;
    ORYON_LOG("Oryon siap: %s", g_state.renderer);
    return true;
}

}  /* namespace oryon */
