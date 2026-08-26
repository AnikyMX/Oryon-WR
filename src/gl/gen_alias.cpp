/* Dihasilkan oleh tools/gen_sources.py - JANGAN DIEDIT MANUAL. */
/* Alias ARB/EXT ke nama inti GLES 3.2. */

#include "oryon/oryon.h"
#include "../driver/driver.h"

using namespace oryon;

ORYON_API void glBindFramebufferEXT(GLenum target, GLuint framebuffer) {
    return gles.glBindFramebuffer(target, framebuffer);
}

ORYON_API void glBindRenderbufferEXT(GLenum target, GLuint renderbuffer) {
    return gles.glBindRenderbuffer(target, renderbuffer);
}

ORYON_API void glBlendFuncSeparateEXT(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    return gles.glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}

ORYON_API GLenum glCheckFramebufferStatusEXT(GLenum target) {
    return gles.glCheckFramebufferStatus(target);
}

ORYON_API void glCompileShaderARB(GLhandleARB shaderObj) {
    return gles.glCompileShader(shaderObj);
}

ORYON_API void glDeleteBuffersARB(GLsizei n, const GLuint *buffers) {
    return gles.glDeleteBuffers(n, buffers);
}

ORYON_API void glDeleteFramebuffersEXT(GLsizei n, const GLuint *framebuffers) {
    return gles.glDeleteFramebuffers(n, framebuffers);
}

ORYON_API void glDeleteRenderbuffersEXT(GLsizei n, const GLuint *renderbuffers) {
    return gles.glDeleteRenderbuffers(n, renderbuffers);
}

ORYON_API void glFramebufferRenderbufferEXT(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    return gles.glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

ORYON_API void glFramebufferTexture2DEXT(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    return gles.glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

ORYON_API void glGenBuffersARB(GLsizei n, GLuint *buffers) {
    return gles.glGenBuffers(n, buffers);
}

ORYON_API void glGenFramebuffersEXT(GLsizei n, GLuint *framebuffers) {
    return gles.glGenFramebuffers(n, framebuffers);
}

ORYON_API void glGenRenderbuffersEXT(GLsizei n, GLuint *renderbuffers) {
    return gles.glGenRenderbuffers(n, renderbuffers);
}

ORYON_API GLint glGetAttribLocationARB(GLhandleARB programObj, const GLcharARB *name) {
    return gles.glGetAttribLocation(programObj, name);
}

ORYON_API GLint glGetUniformLocationARB(GLhandleARB programObj, const GLcharARB *name) {
    return gles.glGetUniformLocation(programObj, name);
}

ORYON_API void glRenderbufferStorageEXT(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    return gles.glRenderbufferStorage(target, internalformat, width, height);
}

ORYON_API void glUniform1fvARB(GLint location, GLsizei count, const GLfloat *value) {
    return gles.glUniform1fv(location, count, value);
}

ORYON_API void glUniform1iARB(GLint location, GLint v0) {
    return gles.glUniform1i(location, v0);
}

ORYON_API void glUniform1ivARB(GLint location, GLsizei count, const GLint *value) {
    return gles.glUniform1iv(location, count, value);
}

ORYON_API void glUniform2fvARB(GLint location, GLsizei count, const GLfloat *value) {
    return gles.glUniform2fv(location, count, value);
}

ORYON_API void glUniform2ivARB(GLint location, GLsizei count, const GLint *value) {
    return gles.glUniform2iv(location, count, value);
}

ORYON_API void glUniform3fvARB(GLint location, GLsizei count, const GLfloat *value) {
    return gles.glUniform3fv(location, count, value);
}

ORYON_API void glUniform3ivARB(GLint location, GLsizei count, const GLint *value) {
    return gles.glUniform3iv(location, count, value);
}

ORYON_API void glUniform4fvARB(GLint location, GLsizei count, const GLfloat *value) {
    return gles.glUniform4fv(location, count, value);
}

ORYON_API void glUniform4ivARB(GLint location, GLsizei count, const GLint *value) {
    return gles.glUniform4iv(location, count, value);
}

ORYON_API void glUniformMatrix2fvARB(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    return gles.glUniformMatrix2fv(location, count, transpose, value);
}

ORYON_API void glUniformMatrix3fvARB(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    return gles.glUniformMatrix3fv(location, count, transpose, value);
}

ORYON_API void glUniformMatrix4fvARB(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    return gles.glUniformMatrix4fv(location, count, transpose, value);
}
