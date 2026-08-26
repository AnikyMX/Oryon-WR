/* Titik gambar. Di sinilah state fixed-function berubah jadi program,
   array sisi klien berubah jadi atribut, dan GL_QUADS berubah jadi segitiga.

   ORYON_IMPL(glDrawArrays)
   ORYON_IMPL(glDrawRangeElements)
   ORYON_IMPL(glMultiDrawArrays)
   ORYON_IMPL(glMultiDrawElements)
*/

#include "oryon/oryon.h"
#include "../draw/vertexpipe.h"
#include "../state/state.h"

using namespace oryon;

ORYON_API void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    if (!ensure_init()) return;
    draw_arrays(mode, first, count);
}

/* glDrawElements sengaja TIDAK diekspor: 1.12.2 tidak pernah memanggilnya
   (WorldVertexBufferUploader memakai glDrawArrays), dan tidak ada flag
   kapabilitas dalam profil yang menuntutnya. Mesinnya sudah ada di
   draw_elements(); menambahkan ekspornya cukup tiga baris bila lingkup
   diperluas ke 1.13+ atau ke mod. */

ORYON_API void glDrawRangeElements(GLenum mode, GLuint start, GLuint end,
                                   GLsizei count, GLenum type,
                                   const GLvoid *indices) {
    if (!ensure_init()) return;
    draw_elements(mode, count, type, indices);
}

ORYON_API void glMultiDrawArrays(GLenum mode, const GLint *first,
                                 const GLsizei *count, GLsizei drawcount) {
    if (!ensure_init() || !first || !count) return;
    for (GLsizei i = 0; i < drawcount; ++i) draw_arrays(mode, first[i], count[i]);
}

ORYON_API void glMultiDrawElements(GLenum mode, const GLsizei *count, GLenum type,
                                   const void *const *indices, GLsizei drawcount) {
    if (!ensure_init() || !count || !indices) return;
    for (GLsizei i = 0; i < drawcount; ++i)
        draw_elements(mode, count[i], type, indices[i]);
}
