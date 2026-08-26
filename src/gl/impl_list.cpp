/* Display list. Minecraft 1.12.2 memakainya untuk langit dan bintang -
   geometri statis yang digambar ulang tiap frame. Yang direkam adalah gambar
   beserta datanya, bukan perintah GL sembarang, karena hanya itu yang pernah
   ditaruh 1.12.2 di dalam sebuah list.

   ORYON_IMPL(glGenLists)
   ORYON_IMPL(glDeleteLists)
   ORYON_IMPL(glNewList)
   ORYON_IMPL(glEndList)
   ORYON_IMPL(glCallList)
*/

#include "oryon/oryon.h"
#include "../draw/vertexpipe.h"
#include "../state/state.h"

using namespace oryon;

ORYON_API GLuint glGenLists(GLsizei range) {
    if (!ensure_init()) return 0;
    return list_gen(range);
}

ORYON_API void glDeleteLists(GLuint list, GLsizei range) {
    if (ensure_init()) list_delete(list, range);
}

/* mode: GL_COMPILE (satu-satunya yang dipakai 1.12.2) atau
   GL_COMPILE_AND_EXECUTE, yang juga didukung. */
ORYON_API void glNewList(GLuint list, GLenum mode) {
    if (!ensure_init()) return;
    if (list_compiling()) { set_error(GL_INVALID_OPERATION); return; }
    if (!list_new(list, mode)) set_error(GL_OUT_OF_MEMORY);
}

ORYON_API void glEndList(void) {
    if (!list_compiling()) { set_error(GL_INVALID_OPERATION); return; }
    list_end();
}

ORYON_API void glCallList(GLuint list) {
    if (ensure_init()) list_call(list);
}
