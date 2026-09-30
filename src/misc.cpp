// Oryon -- small desktop-only entry points with a direct ES mapping (draw buffer, queries, 1D/3D attachments, ...).
#include "oryon.hpp"

using namespace ory;

static GLenum map_query(GLenum target) { return target == GL_SAMPLES_PASSED ? GL_ANY_SAMPLES_PASSED : target; }

/* jar: GL11C.glDrawBuffer(I)V */
OGL_EXPORT void glDrawBuffer(GLenum buf) {
    ORY_PROLOGUE();
    GLint fbo = 0; es.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
    GLenum b = buf;
    if (!fbo) b = (buf == GL_NONE) ? GL_NONE : GL_BACK;                 // default framebuffer: BACK or NONE only
    else if (buf == GL_BACK || buf == GL_FRONT || buf == GL_FRONT_AND_BACK || buf == GL_BACK_LEFT || buf == GL_FRONT_LEFT) b = GL_COLOR_ATTACHMENT0;
    es.glDrawBuffers(1, &b);
}
/* jar: GL15C.glBeginQuery(II)V */
OGL_EXPORT void glBeginQuery(GLenum target, GLuint id) { ORY_PROLOGUE(); es.glBeginQuery(map_query(target), id); }
/* jar: GL15C.glEndQuery(I)V */
OGL_EXPORT void glEndQuery(GLenum target) { ORY_PROLOGUE(); es.glEndQuery(map_query(target)); }
/* jar: GL15C.nglGetQueryiv(IIJ)V */
OGL_EXPORT void glGetQueryiv(GLenum target, GLenum pname, GLint *params) {
    ORY_PROLOGUE();
    if (pname == GL_QUERY_COUNTER_BITS) { *params = target == GL_SAMPLES_PASSED ? 1 : 0; return; }
    es.glGetQueryiv(map_query(target), pname, params);
}
/* jar: GL15C.nglGetQueryObjectiv(IIJ)V */
OGL_EXPORT void glGetQueryObjectiv(GLuint id, GLenum pname, GLint *params) {
    ORY_PROLOGUE(); GLuint v = 0; es.glGetQueryObjectuiv(id, pname, &v); *params = (GLint)v;
}
/* jar: GL30C.glFramebufferTexture1D(IIIII)V */
OGL_EXPORT void glFramebufferTexture1D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    ORY_PROLOGUE(); (void)textarget; es.glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, texture, level);   // 1D is stored as 2D
}
/* jar: GL30C.glFramebufferTexture3D(IIIIII)V */
OGL_EXPORT void glFramebufferTexture3D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset) {
    ORY_PROLOGUE(); (void)textarget; es.glFramebufferTextureLayer(target, attachment, texture, level, zoffset);
}
/* jar: GL11.nglAreTexturesResident(IJJ)Z */
OGL_EXPORT GLboolean glAreTexturesResident(GLsizei n, const GLuint *textures, GLboolean *residences) {
    ORY_PROLOGUE(); (void)textures; for (GLsizei i = 0; i < n; ++i) residences[i] = GL_TRUE; return GL_TRUE;
}
/* jar: GL30C.glClampColor(II)V */
OGL_EXPORT void glClampColor(GLenum target, GLenum clamp) { ORY_PROLOGUE(); (void)target; (void)clamp; }   // ES never clamps float targets
/* jar: GL30C.glBeginConditionalRender(II)V */
OGL_EXPORT void glBeginConditionalRender(GLuint id, GLenum mode) { ORY_PROLOGUE(); (void)id; (void)mode; }   // render unconditionally
/* jar: GL30C.glEndConditionalRender()V */
OGL_EXPORT void glEndConditionalRender(void) { ORY_PROLOGUE(); }
/* jar: GL14C.glPointParameterf(IF)V */
OGL_EXPORT void glPointParameterf(GLenum pname, GLfloat param) { ORY_PROLOGUE(); (void)pname; (void)param; }
/* jar: GL14C.glPointParameteri(II)V */
OGL_EXPORT void glPointParameteri(GLenum pname, GLint param) { ORY_PROLOGUE(); (void)pname; (void)param; }
/* jar: GL14C.nglPointParameterfv(IJ)V */
OGL_EXPORT void glPointParameterfv(GLenum pname, const GLfloat *params) { ORY_PROLOGUE(); (void)pname; (void)params; }
/* jar: GL14C.nglPointParameteriv(IJ)V */
OGL_EXPORT void glPointParameteriv(GLenum pname, const GLint *params) { ORY_PROLOGUE(); (void)pname; (void)params; }
