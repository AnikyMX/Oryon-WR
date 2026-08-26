/* Dihasilkan oleh tools/gen_sources.py - JANGAN DIEDIT MANUAL. */
/* Passthrough murni: identik di GLES 3.2. */

#include "oryon/oryon.h"
#include "../driver/driver.h"

using namespace oryon;

ORYON_API void glAttachShader(GLuint program, GLuint shader) {
    return gles.glAttachShader(program, shader);
}

ORYON_API void glBeginQuery(GLenum target, GLuint id) {
    return gles.glBeginQuery(target, id);
}

ORYON_API void glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) {
    return gles.glBindAttribLocation(program, index, name);
}

ORYON_API void glBindFramebuffer(GLenum target, GLuint framebuffer) {
    return gles.glBindFramebuffer(target, framebuffer);
}

ORYON_API void glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    return gles.glBindRenderbuffer(target, renderbuffer);
}

ORYON_API void glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    return gles.glBlendColor(red, green, blue, alpha);
}

ORYON_API void glBlendEquation(GLenum mode) {
    return gles.glBlendEquation(mode);
}

ORYON_API void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    return gles.glBlendEquationSeparate(modeRGB, modeAlpha);
}

ORYON_API void glBlendFunc(GLenum sfactor, GLenum dfactor) {
    return gles.glBlendFunc(sfactor, dfactor);
}

ORYON_API void glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
    return gles.glBlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}

ORYON_API void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    return gles.glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

ORYON_API GLenum glCheckFramebufferStatus(GLenum target) {
    return gles.glCheckFramebufferStatus(target);
}

ORYON_API void glClear(GLbitfield mask) {
    return gles.glClear(mask);
}

ORYON_API void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    return gles.glClearColor(red, green, blue, alpha);
}

ORYON_API void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    return gles.glColorMask(red, green, blue, alpha);
}

ORYON_API void glCompileShader(GLuint shader) {
    return gles.glCompileShader(shader);
}

ORYON_API void glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data) {
    return gles.glCompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data);
}

ORYON_API void glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data) {
    return gles.glCompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}

ORYON_API void glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    return gles.glCopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}

ORYON_API GLuint glCreateProgram(void) {
    return gles.glCreateProgram();
}

ORYON_API GLuint glCreateShader(GLenum type) {
    return gles.glCreateShader(type);
}

ORYON_API void glCullFace(GLenum mode) {
    return gles.glCullFace(mode);
}

ORYON_API void glDeleteBuffers(GLsizei n, const GLuint *buffers) {
    return gles.glDeleteBuffers(n, buffers);
}

ORYON_API void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) {
    return gles.glDeleteFramebuffers(n, framebuffers);
}

ORYON_API void glDeleteProgram(GLuint program) {
    return gles.glDeleteProgram(program);
}

ORYON_API void glDeleteQueries(GLsizei n, const GLuint *ids) {
    return gles.glDeleteQueries(n, ids);
}

ORYON_API void glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers) {
    return gles.glDeleteRenderbuffers(n, renderbuffers);
}

ORYON_API void glDeleteShader(GLuint shader) {
    return gles.glDeleteShader(shader);
}

ORYON_API void glDeleteTextures(GLsizei n, const GLuint *textures) {
    return gles.glDeleteTextures(n, textures);
}

ORYON_API void glDepthFunc(GLenum func) {
    return gles.glDepthFunc(func);
}

ORYON_API void glDepthMask(GLboolean flag) {
    return gles.glDepthMask(flag);
}

ORYON_API void glDetachShader(GLuint program, GLuint shader) {
    return gles.glDetachShader(program, shader);
}

ORYON_API void glDisableVertexAttribArray(GLuint index) {
    return gles.glDisableVertexAttribArray(index);
}

ORYON_API void glDrawBuffers(GLsizei n, const GLenum *bufs) {
    return gles.glDrawBuffers(n, bufs);
}

ORYON_API void glEnableVertexAttribArray(GLuint index) {
    return gles.glEnableVertexAttribArray(index);
}

ORYON_API void glEndQuery(GLenum target) {
    return gles.glEndQuery(target);
}

ORYON_API void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    return gles.glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

ORYON_API void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    return gles.glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

ORYON_API void glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    return gles.glFramebufferTextureLayer(target, attachment, texture, level, layer);
}

ORYON_API void glGenBuffers(GLsizei n, GLuint *buffers) {
    return gles.glGenBuffers(n, buffers);
}

ORYON_API void glGenFramebuffers(GLsizei n, GLuint *framebuffers) {
    return gles.glGenFramebuffers(n, framebuffers);
}

ORYON_API void glGenQueries(GLsizei n, GLuint *ids) {
    return gles.glGenQueries(n, ids);
}

ORYON_API void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers) {
    return gles.glGenRenderbuffers(n, renderbuffers);
}

ORYON_API void glGenTextures(GLsizei n, GLuint *textures) {
    return gles.glGenTextures(n, textures);
}

ORYON_API void glGenerateMipmap(GLenum target) {
    return gles.glGenerateMipmap(target);
}

ORYON_API void glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {
    return gles.glGetActiveAttrib(program, index, bufSize, length, size, type, name);
}

ORYON_API void glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) {
    return gles.glGetActiveUniform(program, index, bufSize, length, size, type, name);
}

ORYON_API void glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders) {
    return gles.glGetAttachedShaders(program, maxCount, count, shaders);
}

ORYON_API GLint glGetAttribLocation(GLuint program, const GLchar *name) {
    return gles.glGetAttribLocation(program, name);
}

ORYON_API void glGetBufferPointerv(GLenum target, GLenum pname, void **params) {
    return gles.glGetBufferPointerv(target, pname, params);
}

ORYON_API void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params) {
    return gles.glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

ORYON_API void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    return gles.glGetProgramInfoLog(program, bufSize, length, infoLog);
}

ORYON_API void glGetProgramiv(GLuint program, GLenum pname, GLint *params) {
    return gles.glGetProgramiv(program, pname, params);
}

ORYON_API void glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params) {
    return gles.glGetQueryObjectuiv(id, pname, params);
}

ORYON_API void glGetQueryiv(GLenum target, GLenum pname, GLint *params) {
    return gles.glGetQueryiv(target, pname, params);
}

ORYON_API void glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    return gles.glGetRenderbufferParameteriv(target, pname, params);
}

ORYON_API void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    return gles.glGetShaderInfoLog(shader, bufSize, length, infoLog);
}

ORYON_API void glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source) {
    return gles.glGetShaderSource(shader, bufSize, length, source);
}

ORYON_API void glGetShaderiv(GLuint shader, GLenum pname, GLint *params) {
    return gles.glGetShaderiv(shader, pname, params);
}

ORYON_API GLint glGetUniformLocation(GLuint program, const GLchar *name) {
    return gles.glGetUniformLocation(program, name);
}

ORYON_API void glGetUniformfv(GLuint program, GLint location, GLfloat *params) {
    return gles.glGetUniformfv(program, location, params);
}

ORYON_API void glGetUniformiv(GLuint program, GLint location, GLint *params) {
    return gles.glGetUniformiv(program, location, params);
}

ORYON_API void glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer) {
    return gles.glGetVertexAttribPointerv(index, pname, pointer);
}

ORYON_API void glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params) {
    return gles.glGetVertexAttribfv(index, pname, params);
}

ORYON_API void glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params) {
    return gles.glGetVertexAttribiv(index, pname, params);
}

ORYON_API GLboolean glIsBuffer(GLuint buffer) {
    return gles.glIsBuffer(buffer);
}

ORYON_API GLboolean glIsFramebuffer(GLuint framebuffer) {
    return gles.glIsFramebuffer(framebuffer);
}

ORYON_API GLboolean glIsProgram(GLuint program) {
    return gles.glIsProgram(program);
}

ORYON_API GLboolean glIsQuery(GLuint id) {
    return gles.glIsQuery(id);
}

ORYON_API GLboolean glIsRenderbuffer(GLuint renderbuffer) {
    return gles.glIsRenderbuffer(renderbuffer);
}

ORYON_API GLboolean glIsShader(GLuint shader) {
    return gles.glIsShader(shader);
}

ORYON_API void glLineWidth(GLfloat width) {
    return gles.glLineWidth(width);
}

ORYON_API void glPolygonOffset(GLfloat factor, GLfloat units) {
    return gles.glPolygonOffset(factor, units);
}

ORYON_API void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    return gles.glRenderbufferStorage(target, internalformat, width, height);
}

ORYON_API void glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    return gles.glRenderbufferStorageMultisample(target, samples, internalformat, width, height);
}

ORYON_API void glSampleCoverage(GLclampf value, GLboolean invert) {
    return gles.glSampleCoverage(value, invert);
}

ORYON_API void glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    return gles.glStencilFuncSeparate(face, func, ref, mask);
}

ORYON_API void glStencilMaskSeparate(GLenum face, GLuint mask) {
    return gles.glStencilMaskSeparate(face, mask);
}

ORYON_API void glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    return gles.glStencilOpSeparate(face, sfail, dpfail, dppass);
}

ORYON_API void glUniform1f(GLint location, GLfloat v0) {
    return gles.glUniform1f(location, v0);
}

ORYON_API void glUniform1fv(GLint location, GLsizei count, const GLfloat *value) {
    return gles.glUniform1fv(location, count, value);
}

ORYON_API void glUniform1i(GLint location, GLint v0) {
    return gles.glUniform1i(location, v0);
}

ORYON_API void glUniform1iv(GLint location, GLsizei count, const GLint *value) {
    return gles.glUniform1iv(location, count, value);
}

ORYON_API void glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    return gles.glUniform2f(location, v0, v1);
}

ORYON_API void glUniform2fv(GLint location, GLsizei count, const GLfloat *value) {
    return gles.glUniform2fv(location, count, value);
}

ORYON_API void glUniform2i(GLint location, GLint v0, GLint v1) {
    return gles.glUniform2i(location, v0, v1);
}

ORYON_API void glUniform2iv(GLint location, GLsizei count, const GLint *value) {
    return gles.glUniform2iv(location, count, value);
}

ORYON_API void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    return gles.glUniform3f(location, v0, v1, v2);
}

ORYON_API void glUniform3fv(GLint location, GLsizei count, const GLfloat *value) {
    return gles.glUniform3fv(location, count, value);
}

ORYON_API void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
    return gles.glUniform3i(location, v0, v1, v2);
}

ORYON_API void glUniform3iv(GLint location, GLsizei count, const GLint *value) {
    return gles.glUniform3iv(location, count, value);
}

ORYON_API void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    return gles.glUniform4f(location, v0, v1, v2, v3);
}

ORYON_API void glUniform4fv(GLint location, GLsizei count, const GLfloat *value) {
    return gles.glUniform4fv(location, count, value);
}

ORYON_API void glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    return gles.glUniform4i(location, v0, v1, v2, v3);
}

ORYON_API void glUniform4iv(GLint location, GLsizei count, const GLint *value) {
    return gles.glUniform4iv(location, count, value);
}

ORYON_API void glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    return gles.glUniformMatrix2fv(location, count, transpose, value);
}

ORYON_API void glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    return gles.glUniformMatrix2x3fv(location, count, transpose, value);
}

ORYON_API void glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    return gles.glUniformMatrix2x4fv(location, count, transpose, value);
}

ORYON_API void glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    return gles.glUniformMatrix3fv(location, count, transpose, value);
}

ORYON_API void glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    return gles.glUniformMatrix3x2fv(location, count, transpose, value);
}

ORYON_API void glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    return gles.glUniformMatrix3x4fv(location, count, transpose, value);
}

ORYON_API void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    return gles.glUniformMatrix4fv(location, count, transpose, value);
}

ORYON_API void glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    return gles.glUniformMatrix4x2fv(location, count, transpose, value);
}

ORYON_API void glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) {
    return gles.glUniformMatrix4x3fv(location, count, transpose, value);
}

ORYON_API void glValidateProgram(GLuint program) {
    return gles.glValidateProgram(program);
}

ORYON_API void glVertexAttrib1f(GLuint index, GLfloat x) {
    return gles.glVertexAttrib1f(index, x);
}

ORYON_API void glVertexAttrib1fv(GLuint index, const GLfloat *v) {
    return gles.glVertexAttrib1fv(index, v);
}

ORYON_API void glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
    return gles.glVertexAttrib2f(index, x, y);
}

ORYON_API void glVertexAttrib2fv(GLuint index, const GLfloat *v) {
    return gles.glVertexAttrib2fv(index, v);
}

ORYON_API void glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    return gles.glVertexAttrib3f(index, x, y, z);
}

ORYON_API void glVertexAttrib3fv(GLuint index, const GLfloat *v) {
    return gles.glVertexAttrib3fv(index, v);
}

ORYON_API void glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    return gles.glVertexAttrib4f(index, x, y, z, w);
}

ORYON_API void glVertexAttrib4fv(GLuint index, const GLfloat *v) {
    return gles.glVertexAttrib4fv(index, v);
}

ORYON_API void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) {
    return gles.glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

ORYON_API void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    return gles.glViewport(x, y, width, height);
}
