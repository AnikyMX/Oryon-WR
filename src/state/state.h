/* State milik Oryon sendiri: yang tidak bisa ditanyakan ke driver GLES. */
#ifndef ORYON_STATE_H
#define ORYON_STATE_H

#include "oryon/oryon.h"

namespace oryon {

/* Pojav menjalankan satu context GL untuk satu proses, dan Minecraft hanya
   menggambar dari satu thread. State disimpan global tanpa TLS supaya tidak
   ada biaya akses per panggilan. Bila suatu saat perlu multi-context, satu-
   satunya perubahan adalah menjadikan g_state penunjuk ber-TLS. */
struct State {
    GLenum error;              /* error milik Oryon, terpisah dari driver */
    const char *version;
    const char *renderer;
    const char *vendor;
    const char *glsl;
    const char *extensions;
};

ORYON_LOCAL extern State g_state;
ORYON_LOCAL void state_init();

/* Semantik GL: error PERTAMA yang bertahan sampai dibaca. */
ORYON_INLINE void set_error(GLenum e) {
    if (g_state.error == GL_NO_ERROR) g_state.error = e;
}

}  /* namespace oryon */

#endif /* ORYON_STATE_H */
