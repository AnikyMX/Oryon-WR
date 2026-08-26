/* Perakit vertex: satu-satunya tempat data vertex bertemu driver.

   GLES 3.2 tidak punya mode langsung maupun array sisi klien, jadi keduanya
   berakhir di sini sebagai buffer streaming plus atribut vertex generik.
   Terjemahan primitif juga di sini - GL_QUADS satu-satunya yang butuh index
   buffer; GL_QUAD_STRIP dan GL_POLYGON ternyata identik dengan padanan GLES-nya. */
#ifndef ORYON_VERTEXPIPE_H
#define ORYON_VERTEXPIPE_H

#include "oryon/oryon.h"
#include "../state/ff_state.h"

namespace oryon {

/* Vertex mode langsung: tata letak tetap, 56 byte.
   Minecraft memakai mode langsung hanya untuk GUI dan debug, jadi menulis
   seluruh medan tiap vertex lebih murah daripada mengurus stride dinamis. */
struct ImmVertex {
    float pos[3];
    float color[4];
    float normal[3];
    float tex[2][2];
};

struct ClientArray {
    const void *ptr;
    GLuint  buffer;      /* pengikatan GL_ARRAY_BUFFER saat ditetapkan */
    GLsizei stride;      /* stride efektif dalam byte, sudah dinormalkan */
    GLint   size;
    GLenum  type;
    bool    enabled;
};

struct ArrayState {
    ClientArray vertex;
    ClientArray color;
    ClientArray normal;
    ClientArray tex[FF_MAX_TEX];
    GLuint      array_buffer;
    GLuint      element_buffer;
};

ORYON_LOCAL extern ArrayState g_arr;

ORYON_LOCAL bool draw_init();

/* --- mode langsung --- */
ORYON_LOCAL void imm_begin(GLenum mode);
ORYON_LOCAL void imm_vertex(float x, float y, float z);
ORYON_LOCAL void imm_end();
ORYON_LOCAL bool imm_active();

/* --- jalur array --- */
ORYON_LOCAL void draw_arrays(GLenum mode, GLint first, GLsizei count);
ORYON_LOCAL void draw_elements(GLenum mode, GLsizei count, GLenum type,
                               const void *indices);

/* --- display list ---
   Sebuah list merekam gambar-gambar, bukan perintah GL sembarang: 1.12.2 hanya
   menaruh geometri di dalamnya (langit, bintang). Data vertexnya diunggah
   sekali ke buffer permanen - itulah nilai sesungguhnya sebuah display list. */
ORYON_LOCAL GLuint list_gen(GLsizei range);
ORYON_LOCAL void   list_delete(GLuint first, GLsizei range);
ORYON_LOCAL bool   list_new(GLuint list, GLenum mode);
ORYON_LOCAL void   list_end();
ORYON_LOCAL void   list_call(GLuint list);
ORYON_LOCAL bool   list_compiling();

}  /* namespace oryon */

#endif /* ORYON_VERTEXPIPE_H */
