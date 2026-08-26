/* Penunjuk fungsi ke driver GLES 3.2 sungguhan. */
#ifndef ORYON_DRIVER_H
#define ORYON_DRIVER_H

#include "oryon/oryon.h"

namespace oryon {

struct GlesProcs {
#define ORYON_GLES(ret, name, params, args) ret (*name) params;
#include "gles_procs.inc"
};

/* Satu tingkat tak langsung per panggilan. Tidak ada pembungkus, tidak ada
   pemeriksaan, tidak ada pencarian simbol di jalur gambar. */
ORYON_LOCAL extern GlesProcs gles;

ORYON_LOCAL bool driver_load();
ORYON_LOCAL void *egl_get_proc(const char *name);

}  /* namespace oryon */

#endif /* ORYON_DRIVER_H */
