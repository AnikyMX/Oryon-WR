/* Buffer object. Hampir semuanya sama persis di GLES; yang perlu campur tangan
   hanya pelacakan pengikatan (dipakai jalur client array) dan glMapBuffer,
   yang di GLES hanya ada sebagai glMapBufferRange.

   ORYON_IMPL(glBindBuffer)
   ORYON_IMPL(glBindBufferARB)
   ORYON_IMPL(glBufferData)
   ORYON_IMPL(glBufferDataARB)
   ORYON_IMPL(glBufferSubData)
   ORYON_IMPL(glGetBufferParameteriv)
   ORYON_IMPL(glMapBuffer)
   ORYON_IMPL(glUnmapBuffer)
   ORYON_IMPL(glGetBufferSubData)
*/

#include "oryon/oryon.h"
#include "../driver/driver.h"
#include "../draw/vertexpipe.h"
#include "../state/state.h"

#include <string.h>

using namespace oryon;

namespace {
void bind(GLenum target, GLuint buffer) {
    if (target == GL_ARRAY_BUFFER)              g_arr.array_buffer = buffer;
    else if (target == GL_ELEMENT_ARRAY_BUFFER) g_arr.element_buffer = buffer;
    gles.glBindBuffer(target, buffer);
}
}  /* namespace */

ORYON_API void glBindBuffer(GLenum target, GLuint buffer) {
    if (ensure_init()) bind(target, buffer);
}

ORYON_API void glBindBufferARB(GLenum target, GLuint buffer) {
    if (ensure_init()) bind(target, buffer);
}

ORYON_API void glBufferData(GLenum target, GLsizeiptr size, const void *data,
                            GLenum usage) {
    gles.glBufferData(target, size, data, usage);
}

ORYON_API void glBufferDataARB(GLenum target, GLsizeiptrARB size, const void *data,
                               GLenum usage) {
    gles.glBufferData(target, (GLsizeiptr) size, data, usage);
}

ORYON_API void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                               const void *data) {
    gles.glBufferSubData(target, offset, size, data);
}

ORYON_API void glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    gles.glGetBufferParameteriv(target, pname, params);
}

/* GLES hanya punya glMapBufferRange. Ukuran buffer ditanyakan ke driver supaya
   rentangnya persis, bukan tebakan. */
ORYON_API void *glMapBuffer(GLenum target, GLenum access) {
    if (!ensure_init()) return 0;
    GLint size = 0;
    gles.glGetBufferParameteriv(target, GL_BUFFER_SIZE, &size);
    if (size <= 0) { set_error(GL_INVALID_OPERATION); return 0; }

    GLbitfield bits = 0;
    if (access == GL_READ_ONLY)  bits = GL_MAP_READ_BIT;
    else if (access == GL_WRITE_ONLY) bits = GL_MAP_WRITE_BIT;
    else bits = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;

    return gles.glMapBufferRange(target, 0, size, bits);
}

ORYON_API GLboolean glUnmapBuffer(GLenum target) {
    return gles.glUnmapBuffer(target);
}

/* Tidak ada padanannya di GLES: dipetakan, disalin, lalu dilepas. */
ORYON_API void glGetBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size,
                                  void *data) {
    if (!ensure_init() || !data || size <= 0) return;
    void *p = gles.glMapBufferRange(target, offset, size, GL_MAP_READ_BIT);
    if (!p) { set_error(GL_INVALID_OPERATION); return; }
    memcpy(data, p, (size_t) size);
    gles.glUnmapBuffer(target);
}
