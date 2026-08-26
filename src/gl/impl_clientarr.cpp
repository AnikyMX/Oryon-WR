/* Array sisi klien. GLES 3.2 tidak punya konsep ini sama sekali: setiap
   penunjuk harus berakhir sebagai atribut vertex generik yang bersumber dari
   buffer object. Penerjemahannya terjadi saat menggambar, bukan di sini -
   di sini hanya dicatat.

   ORYON_IMPL(glEnableClientState)
   ORYON_IMPL(glDisableClientState)
   ORYON_IMPL(glVertexPointer)
   ORYON_IMPL(glColorPointer)
   ORYON_IMPL(glNormalPointer)
   ORYON_IMPL(glTexCoordPointer)
*/

#include "oryon/oryon.h"
#include "../draw/vertexpipe.h"
#include "../state/ff_state.h"
#include "../state/state.h"

using namespace oryon;

namespace {

GLsizei type_size(GLenum t) {
    switch (t) {
    case GL_BYTE: case GL_UNSIGNED_BYTE:   return 1;
    case GL_SHORT: case GL_UNSIGNED_SHORT: case GL_HALF_FLOAT: return 2;
    case GL_DOUBLE:                        return 8;
    default:                               return 4;
    }
}

void set_array(ClientArray &a, GLint size, GLenum type, GLsizei stride,
               const void *ptr) {
    a.size   = size;
    a.type   = type;
    a.stride = stride ? stride : size * type_size(type);
    a.ptr    = ptr;
    a.buffer = g_arr.array_buffer;   /* aturan GL: pengikatan saat ditetapkan */
}

ClientArray *array_for(GLenum cap) {
    switch (cap) {
    case GL_VERTEX_ARRAY:        return &g_arr.vertex;
    case GL_COLOR_ARRAY:         return &g_arr.color;
    case GL_NORMAL_ARRAY:        return &g_arr.normal;
    case GL_TEXTURE_COORD_ARRAY: return &g_arr.tex[g_ff.a.client_tex];
    default:                     return 0;
    }
}

}  /* namespace */

ORYON_API void glEnableClientState(GLenum cap) {
    ClientArray *a = array_for(cap);
    if (a) a->enabled = true;
    else   ORYON_LOG("glEnableClientState(0x%X) diabaikan", cap);
}

ORYON_API void glDisableClientState(GLenum cap) {
    ClientArray *a = array_for(cap);
    if (a) a->enabled = false;
}

ORYON_API void glVertexPointer(GLint size, GLenum type, GLsizei stride,
                               const GLvoid *ptr) {
    set_array(g_arr.vertex, size, type, stride, ptr);
}

ORYON_API void glColorPointer(GLint size, GLenum type, GLsizei stride,
                              const GLvoid *ptr) {
    set_array(g_arr.color, size, type, stride, ptr);
}

/* glNormalPointer tidak punya argumen size: normal selalu tiga komponen. */
ORYON_API void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *ptr) {
    set_array(g_arr.normal, 3, type, stride, ptr);
}

ORYON_API void glTexCoordPointer(GLint size, GLenum type, GLsizei stride,
                                 const GLvoid *ptr) {
    set_array(g_arr.tex[g_ff.a.client_tex], size, type, stride, ptr);
}
