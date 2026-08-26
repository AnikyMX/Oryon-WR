/* Dihasilkan oleh tools/gen_exports.py - JANGAN DIEDIT MANUAL. */
/* Sumber: 1.12.2.jar (call site) + lwjglglfwclasses.jar (simbol) + header GL sistem. */
/*
 * ORYON_SYM(STRATEGI, RET, NAMA, (PARAM), (ARG))
 *
 * STRATEGI:
 *   direct        117
 *   immediate      92
 *   alias          33
 *   translate      19
 *   legacy_misc    19
 *   matrix         15
 *   texture        13
 *   buffer          7
 *   clientarr       6
 *   query           5
 *   texenv          5
 *   displaylist     4
 *   lighting        4
 *   draw            4
 *   fog             3
 *   capbits         2
 *   attribstack     2
 *   alphatest       1
 *   shader          1
 */

#ifndef ORYON_SYM
#error "definisikan ORYON_SYM sebelum menyertakan berkas ini"
#endif


/* ---- texture ---- */
ORYON_SYM(texture, void, glActiveTexture, (GLenum texture), (texture))

/* ---- alias ---- */
ORYON_SYM(alias, void, glActiveTextureARB, (GLenum texture), (texture))

/* ---- alphatest ---- */
ORYON_SYM(alphatest, void, glAlphaFunc, (GLenum func, GLclampf ref), (func, ref))

/* ---- translate ---- */
ORYON_SYM(translate, void, glAttachObjectARB, (GLhandleARB containerObj, GLhandleARB obj), (containerObj, obj))

/* ---- direct ---- */
ORYON_SYM(direct, void, glAttachShader, (GLuint program, GLuint shader), (program, shader))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glBegin, (GLenum mode), (mode))

/* ---- direct ---- */
ORYON_SYM(direct, void, glBeginQuery, (GLenum target, GLuint id), (target, id))
ORYON_SYM(direct, void, glBindAttribLocation, (GLuint program, GLuint index, const GLchar *name), (program, index, name))

/* ---- buffer ---- */
ORYON_SYM(buffer, void, glBindBuffer, (GLenum target, GLuint buffer), (target, buffer))

/* ---- alias ---- */
ORYON_SYM(alias, void, glBindBufferARB, (GLenum target, GLuint buffer), (target, buffer))

/* ---- direct ---- */
ORYON_SYM(direct, void, glBindFramebuffer, (GLenum target, GLuint framebuffer), (target, framebuffer))

/* ---- alias ---- */
ORYON_SYM(alias, void, glBindFramebufferEXT, (GLenum target, GLuint framebuffer), (target, framebuffer))

/* ---- direct ---- */
ORYON_SYM(direct, void, glBindRenderbuffer, (GLenum target, GLuint renderbuffer), (target, renderbuffer))

/* ---- alias ---- */
ORYON_SYM(alias, void, glBindRenderbufferEXT, (GLenum target, GLuint renderbuffer), (target, renderbuffer))

/* ---- texture ---- */
ORYON_SYM(texture, void, glBindTexture, (GLenum target, GLuint texture), (target, texture))

/* ---- direct ---- */
ORYON_SYM(direct, void, glBlendColor, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha), (red, green, blue, alpha))
ORYON_SYM(direct, void, glBlendEquation, (GLenum mode), (mode))
ORYON_SYM(direct, void, glBlendEquationSeparate, (GLenum modeRGB, GLenum modeAlpha), (modeRGB, modeAlpha))
ORYON_SYM(direct, void, glBlendFunc, (GLenum sfactor, GLenum dfactor), (sfactor, dfactor))
ORYON_SYM(direct, void, glBlendFuncSeparate, (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha), (sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha))

/* ---- alias ---- */
ORYON_SYM(alias, void, glBlendFuncSeparateEXT, (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha), (sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha))

/* ---- direct ---- */
ORYON_SYM(direct, void, glBlitFramebuffer, (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter), (srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter))

/* ---- buffer ---- */
ORYON_SYM(buffer, void, glBufferData, (GLenum target, GLsizeiptr size, const void *data, GLenum usage), (target, size, data, usage))

/* ---- alias ---- */
ORYON_SYM(alias, void, glBufferDataARB, (GLenum target, GLsizeiptrARB size, const void *data, GLenum usage), (target, size, data, usage))

/* ---- buffer ---- */
ORYON_SYM(buffer, void, glBufferSubData, (GLenum target, GLintptr offset, GLsizeiptr size, const void *data), (target, offset, size, data))

/* ---- displaylist ---- */
ORYON_SYM(displaylist, void, glCallList, (GLuint list), (list))

/* ---- direct ---- */
ORYON_SYM(direct, GLenum, glCheckFramebufferStatus, (GLenum target), (target))

/* ---- alias ---- */
ORYON_SYM(alias, GLenum, glCheckFramebufferStatusEXT, (GLenum target), (target))

/* ---- direct ---- */
ORYON_SYM(direct, void, glClear, (GLbitfield mask), (mask))
ORYON_SYM(direct, void, glClearColor, (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha), (red, green, blue, alpha))

/* ---- translate ---- */
ORYON_SYM(translate, void, glClearDepth, (GLclampd depth), (depth))

/* ---- clientarr ---- */
ORYON_SYM(clientarr, void, glClientActiveTexture, (GLenum texture), (texture))
ORYON_SYM(clientarr, void, glClientActiveTextureARB, (GLenum texture), (texture))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glColor4f, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha), (red, green, blue, alpha))

/* ---- direct ---- */
ORYON_SYM(direct, void, glColorMask, (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha), (red, green, blue, alpha))

/* ---- lighting ---- */
ORYON_SYM(lighting, void, glColorMaterial, (GLenum face, GLenum mode), (face, mode))

/* ---- clientarr ---- */
ORYON_SYM(clientarr, void, glColorPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr), (size, type, stride, ptr))

/* ---- direct ---- */
ORYON_SYM(direct, void, glCompileShader, (GLuint shader), (shader))

/* ---- alias ---- */
ORYON_SYM(alias, void, glCompileShaderARB, (GLhandleARB shaderObj), (shaderObj))

/* ---- translate ---- */
ORYON_SYM(translate, void, glCompressedTexImage1D, (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data), (target, level, internalformat, width, border, imageSize, data))

/* ---- texture ---- */
ORYON_SYM(texture, void, glCompressedTexImage2D, (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data), (target, level, internalformat, width, height, border, imageSize, data))

/* ---- direct ---- */
ORYON_SYM(direct, void, glCompressedTexImage3D, (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid *data), (target, level, internalformat, width, height, depth, border, imageSize, data))

/* ---- translate ---- */
ORYON_SYM(translate, void, glCompressedTexSubImage1D, (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data), (target, level, xoffset, width, format, imageSize, data))

/* ---- texture ---- */
ORYON_SYM(texture, void, glCompressedTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data), (target, level, xoffset, yoffset, width, height, format, imageSize, data))

/* ---- direct ---- */
ORYON_SYM(direct, void, glCompressedTexSubImage3D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data), (target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data))

/* ---- texture ---- */
ORYON_SYM(texture, void, glCopyTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height), (target, level, xoffset, yoffset, x, y, width, height))

/* ---- direct ---- */
ORYON_SYM(direct, void, glCopyTexSubImage3D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height), (target, level, xoffset, yoffset, zoffset, x, y, width, height))
ORYON_SYM(direct, GLuint, glCreateProgram, (void), ())

/* ---- translate ---- */
ORYON_SYM(translate, GLhandleARB, glCreateProgramObjectARB, (void), ())

/* ---- direct ---- */
ORYON_SYM(direct, GLuint, glCreateShader, (GLenum type), (type))

/* ---- translate ---- */
ORYON_SYM(translate, GLhandleARB, glCreateShaderObjectARB, (GLenum shaderType), (shaderType))

/* ---- direct ---- */
ORYON_SYM(direct, void, glCullFace, (GLenum mode), (mode))
ORYON_SYM(direct, void, glDeleteBuffers, (GLsizei n, const GLuint *buffers), (n, buffers))

/* ---- alias ---- */
ORYON_SYM(alias, void, glDeleteBuffersARB, (GLsizei n, const GLuint *buffers), (n, buffers))

/* ---- direct ---- */
ORYON_SYM(direct, void, glDeleteFramebuffers, (GLsizei n, const GLuint *framebuffers), (n, framebuffers))

/* ---- alias ---- */
ORYON_SYM(alias, void, glDeleteFramebuffersEXT, (GLsizei n, const GLuint *framebuffers), (n, framebuffers))

/* ---- displaylist ---- */
ORYON_SYM(displaylist, void, glDeleteLists, (GLuint list, GLsizei range), (list, range))

/* ---- translate ---- */
ORYON_SYM(translate, void, glDeleteObjectARB, (GLhandleARB obj), (obj))

/* ---- direct ---- */
ORYON_SYM(direct, void, glDeleteProgram, (GLuint program), (program))
ORYON_SYM(direct, void, glDeleteQueries, (GLsizei n, const GLuint *ids), (n, ids))
ORYON_SYM(direct, void, glDeleteRenderbuffers, (GLsizei n, const GLuint *renderbuffers), (n, renderbuffers))

/* ---- alias ---- */
ORYON_SYM(alias, void, glDeleteRenderbuffersEXT, (GLsizei n, const GLuint *renderbuffers), (n, renderbuffers))

/* ---- direct ---- */
ORYON_SYM(direct, void, glDeleteShader, (GLuint shader), (shader))
ORYON_SYM(direct, void, glDeleteTextures, (GLsizei n, const GLuint *textures), (n, textures))
ORYON_SYM(direct, void, glDepthFunc, (GLenum func), (func))
ORYON_SYM(direct, void, glDepthMask, (GLboolean flag), (flag))
ORYON_SYM(direct, void, glDetachShader, (GLuint program, GLuint shader), (program, shader))

/* ---- capbits ---- */
ORYON_SYM(capbits, void, glDisable, (GLenum cap), (cap))

/* ---- clientarr ---- */
ORYON_SYM(clientarr, void, glDisableClientState, (GLenum cap), (cap))

/* ---- direct ---- */
ORYON_SYM(direct, void, glDisableVertexAttribArray, (GLuint index), (index))

/* ---- draw ---- */
ORYON_SYM(draw, void, glDrawArrays, (GLenum mode, GLint first, GLsizei count), (mode, first, count))

/* ---- direct ---- */
ORYON_SYM(direct, void, glDrawBuffers, (GLsizei n, const GLenum *bufs), (n, bufs))

/* ---- draw ---- */
ORYON_SYM(draw, void, glDrawRangeElements, (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices), (mode, start, end, count, type, indices))

/* ---- capbits ---- */
ORYON_SYM(capbits, void, glEnable, (GLenum cap), (cap))

/* ---- clientarr ---- */
ORYON_SYM(clientarr, void, glEnableClientState, (GLenum cap), (cap))

/* ---- direct ---- */
ORYON_SYM(direct, void, glEnableVertexAttribArray, (GLuint index), (index))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glEnd, (void), ())
ORYON_SYM(immediate, void, glEndList, (void), ())

/* ---- direct ---- */
ORYON_SYM(direct, void, glEndQuery, (GLenum target), (target))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glFogCoordPointer, (GLenum type, GLsizei stride, const void *pointer), (type, stride, pointer))
ORYON_SYM(immediate, void, glFogCoordd, (GLdouble coord), (coord))
ORYON_SYM(immediate, void, glFogCoorddv, (const GLdouble *coord), (coord))
ORYON_SYM(immediate, void, glFogCoordf, (GLfloat coord), (coord))
ORYON_SYM(immediate, void, glFogCoordfv, (const GLfloat *coord), (coord))

/* ---- fog ---- */
ORYON_SYM(fog, void, glFogf, (GLenum pname, GLfloat param), (pname, param))
ORYON_SYM(fog, void, glFogfv, (GLenum pname, const GLfloat *params), (pname, params))
ORYON_SYM(fog, void, glFogi, (GLenum pname, GLint param), (pname, param))

/* ---- direct ---- */
ORYON_SYM(direct, void, glFramebufferRenderbuffer, (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer), (target, attachment, renderbuffertarget, renderbuffer))

/* ---- alias ---- */
ORYON_SYM(alias, void, glFramebufferRenderbufferEXT, (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer), (target, attachment, renderbuffertarget, renderbuffer))

/* ---- translate ---- */
ORYON_SYM(translate, void, glFramebufferTexture1D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level), (target, attachment, textarget, texture, level))

/* ---- direct ---- */
ORYON_SYM(direct, void, glFramebufferTexture2D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level), (target, attachment, textarget, texture, level))

/* ---- alias ---- */
ORYON_SYM(alias, void, glFramebufferTexture2DEXT, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level), (target, attachment, textarget, texture, level))

/* ---- translate ---- */
ORYON_SYM(translate, void, glFramebufferTexture3D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset), (target, attachment, textarget, texture, level, zoffset))

/* ---- direct ---- */
ORYON_SYM(direct, void, glFramebufferTextureLayer, (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer), (target, attachment, texture, level, layer))
ORYON_SYM(direct, void, glGenBuffers, (GLsizei n, GLuint *buffers), (n, buffers))

/* ---- alias ---- */
ORYON_SYM(alias, void, glGenBuffersARB, (GLsizei n, GLuint *buffers), (n, buffers))

/* ---- direct ---- */
ORYON_SYM(direct, void, glGenFramebuffers, (GLsizei n, GLuint *framebuffers), (n, framebuffers))

/* ---- alias ---- */
ORYON_SYM(alias, void, glGenFramebuffersEXT, (GLsizei n, GLuint *framebuffers), (n, framebuffers))

/* ---- displaylist ---- */
ORYON_SYM(displaylist, GLuint, glGenLists, (GLsizei range), (range))

/* ---- direct ---- */
ORYON_SYM(direct, void, glGenQueries, (GLsizei n, GLuint *ids), (n, ids))
ORYON_SYM(direct, void, glGenRenderbuffers, (GLsizei n, GLuint *renderbuffers), (n, renderbuffers))

/* ---- alias ---- */
ORYON_SYM(alias, void, glGenRenderbuffersEXT, (GLsizei n, GLuint *renderbuffers), (n, renderbuffers))

/* ---- direct ---- */
ORYON_SYM(direct, void, glGenTextures, (GLsizei n, GLuint *textures), (n, textures))
ORYON_SYM(direct, void, glGenerateMipmap, (GLenum target), (target))
ORYON_SYM(direct, void, glGetActiveAttrib, (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name), (program, index, bufSize, length, size, type, name))
ORYON_SYM(direct, void, glGetActiveUniform, (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name), (program, index, bufSize, length, size, type, name))
ORYON_SYM(direct, void, glGetAttachedShaders, (GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders), (program, maxCount, count, shaders))
ORYON_SYM(direct, GLint, glGetAttribLocation, (GLuint program, const GLchar *name), (program, name))

/* ---- alias ---- */
ORYON_SYM(alias, GLint, glGetAttribLocationARB, (GLhandleARB programObj, const GLcharARB *name), (programObj, name))

/* ---- buffer ---- */
ORYON_SYM(buffer, void, glGetBufferParameteriv, (GLenum target, GLenum pname, GLint *params), (target, pname, params))

/* ---- direct ---- */
ORYON_SYM(direct, void, glGetBufferPointerv, (GLenum target, GLenum pname, void **params), (target, pname, params))

/* ---- buffer ---- */
ORYON_SYM(buffer, void, glGetBufferSubData, (GLenum target, GLintptr offset, GLsizeiptr size, void *data), (target, offset, size, data))

/* ---- translate ---- */
ORYON_SYM(translate, void, glGetCompressedTexImage, (GLenum target, GLint lod, GLvoid *img), (target, lod, img))

/* ---- query ---- */
ORYON_SYM(query, GLenum, glGetError, (void), ())
ORYON_SYM(query, void, glGetFloatv, (GLenum pname, GLfloat *params), (pname, params))

/* ---- direct ---- */
ORYON_SYM(direct, void, glGetFramebufferAttachmentParameteriv, (GLenum target, GLenum attachment, GLenum pname, GLint *params), (target, attachment, pname, params))

/* ---- translate ---- */
ORYON_SYM(translate, void, glGetInfoLogARB, (GLhandleARB obj, GLsizei maxLength, GLsizei *length, GLcharARB *infoLog), (obj, maxLength, length, infoLog))

/* ---- query ---- */
ORYON_SYM(query, void, glGetIntegerv, (GLenum pname, GLint *params), (pname, params))

/* ---- translate ---- */
ORYON_SYM(translate, void, glGetObjectParameterivARB, (GLhandleARB obj, GLenum pname, GLint *params), (obj, pname, params))

/* ---- direct ---- */
ORYON_SYM(direct, void, glGetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog), (program, bufSize, length, infoLog))
ORYON_SYM(direct, void, glGetProgramiv, (GLuint program, GLenum pname, GLint *params), (program, pname, params))

/* ---- translate ---- */
ORYON_SYM(translate, void, glGetQueryObjectiv, (GLuint id, GLenum pname, GLint *params), (id, pname, params))

/* ---- direct ---- */
ORYON_SYM(direct, void, glGetQueryObjectuiv, (GLuint id, GLenum pname, GLuint *params), (id, pname, params))
ORYON_SYM(direct, void, glGetQueryiv, (GLenum target, GLenum pname, GLint *params), (target, pname, params))
ORYON_SYM(direct, void, glGetRenderbufferParameteriv, (GLenum target, GLenum pname, GLint *params), (target, pname, params))
ORYON_SYM(direct, void, glGetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog), (shader, bufSize, length, infoLog))
ORYON_SYM(direct, void, glGetShaderSource, (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source), (shader, bufSize, length, source))
ORYON_SYM(direct, void, glGetShaderiv, (GLuint shader, GLenum pname, GLint *params), (shader, pname, params))

/* ---- query ---- */
ORYON_SYM(query, const GLubyte*, glGetString, (GLenum name), (name))

/* ---- legacy_misc ---- */
ORYON_SYM(legacy_misc, void, glGetTexImage, (GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels), (target, level, format, type, pixels))

/* ---- query ---- */
ORYON_SYM(query, void, glGetTexLevelParameteriv, (GLenum target, GLint level, GLenum pname, GLint *params), (target, level, pname, params))

/* ---- direct ---- */
ORYON_SYM(direct, GLint, glGetUniformLocation, (GLuint program, const GLchar *name), (program, name))

/* ---- alias ---- */
ORYON_SYM(alias, GLint, glGetUniformLocationARB, (GLhandleARB programObj, const GLcharARB *name), (programObj, name))

/* ---- direct ---- */
ORYON_SYM(direct, void, glGetUniformfv, (GLuint program, GLint location, GLfloat *params), (program, location, params))
ORYON_SYM(direct, void, glGetUniformiv, (GLuint program, GLint location, GLint *params), (program, location, params))
ORYON_SYM(direct, void, glGetVertexAttribPointerv, (GLuint index, GLenum pname, void **pointer), (index, pname, pointer))

/* ---- translate ---- */
ORYON_SYM(translate, void, glGetVertexAttribdv, (GLuint index, GLenum pname, GLdouble *params), (index, pname, params))

/* ---- direct ---- */
ORYON_SYM(direct, void, glGetVertexAttribfv, (GLuint index, GLenum pname, GLfloat *params), (index, pname, params))
ORYON_SYM(direct, void, glGetVertexAttribiv, (GLuint index, GLenum pname, GLint *params), (index, pname, params))
ORYON_SYM(direct, GLboolean, glIsBuffer, (GLuint buffer), (buffer))
ORYON_SYM(direct, GLboolean, glIsFramebuffer, (GLuint framebuffer), (framebuffer))
ORYON_SYM(direct, GLboolean, glIsProgram, (GLuint program), (program))
ORYON_SYM(direct, GLboolean, glIsQuery, (GLuint id), (id))
ORYON_SYM(direct, GLboolean, glIsRenderbuffer, (GLuint renderbuffer), (renderbuffer))
ORYON_SYM(direct, GLboolean, glIsShader, (GLuint shader), (shader))

/* ---- lighting ---- */
ORYON_SYM(lighting, void, glLightModelfv, (GLenum pname, const GLfloat *params), (pname, params))
ORYON_SYM(lighting, void, glLightfv, (GLenum light, GLenum pname, const GLfloat *params), (light, pname, params))

/* ---- direct ---- */
ORYON_SYM(direct, void, glLineWidth, (GLfloat width), (width))
ORYON_SYM(direct, void, glLinkProgram, (GLuint program), (program))

/* ---- alias ---- */
ORYON_SYM(alias, void, glLinkProgramARB, (GLhandleARB programObj), (programObj))

/* ---- matrix ---- */
ORYON_SYM(matrix, void, glLoadIdentity, (void), ())
ORYON_SYM(matrix, void, glLoadTransposeMatrixd, (const GLdouble m[16]), (m))
ORYON_SYM(matrix, void, glLoadTransposeMatrixf, (const GLfloat m[16]), (m))

/* ---- legacy_misc ---- */
ORYON_SYM(legacy_misc, void, glLogicOp, (GLenum opcode), (opcode))

/* ---- buffer ---- */
ORYON_SYM(buffer, void *, glMapBuffer, (GLenum target, GLenum access), (target, access))

/* ---- matrix ---- */
ORYON_SYM(matrix, void, glMatrixMode, (GLenum mode), (mode))
ORYON_SYM(matrix, void, glMultMatrixf, (const GLfloat *m), (m))
ORYON_SYM(matrix, void, glMultTransposeMatrixd, (const GLdouble m[16]), (m))
ORYON_SYM(matrix, void, glMultTransposeMatrixf, (const GLfloat m[16]), (m))

/* ---- draw ---- */
ORYON_SYM(draw, void, glMultiDrawArrays, (GLenum mode, const GLint *first, const GLsizei *count, GLsizei drawcount), (mode, first, count, drawcount))
ORYON_SYM(draw, void, glMultiDrawElements, (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei drawcount), (mode, count, type, indices, drawcount))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glMultiTexCoord1d, (GLenum target, GLdouble s), (target, s))
ORYON_SYM(immediate, void, glMultiTexCoord1dv, (GLenum target, const GLdouble *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord1f, (GLenum target, GLfloat s), (target, s))
ORYON_SYM(immediate, void, glMultiTexCoord1fv, (GLenum target, const GLfloat *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord1i, (GLenum target, GLint s), (target, s))
ORYON_SYM(immediate, void, glMultiTexCoord1iv, (GLenum target, const GLint *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord1s, (GLenum target, GLshort s), (target, s))
ORYON_SYM(immediate, void, glMultiTexCoord1sv, (GLenum target, const GLshort *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord2d, (GLenum target, GLdouble s, GLdouble t), (target, s, t))
ORYON_SYM(immediate, void, glMultiTexCoord2dv, (GLenum target, const GLdouble *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord2f, (GLenum target, GLfloat s, GLfloat t), (target, s, t))
ORYON_SYM(immediate, void, glMultiTexCoord2fARB, (GLenum target, GLfloat s, GLfloat t), (target, s, t))
ORYON_SYM(immediate, void, glMultiTexCoord2fv, (GLenum target, const GLfloat *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord2i, (GLenum target, GLint s, GLint t), (target, s, t))
ORYON_SYM(immediate, void, glMultiTexCoord2iv, (GLenum target, const GLint *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord2s, (GLenum target, GLshort s, GLshort t), (target, s, t))
ORYON_SYM(immediate, void, glMultiTexCoord2sv, (GLenum target, const GLshort *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord3d, (GLenum target, GLdouble s, GLdouble t, GLdouble r), (target, s, t, r))
ORYON_SYM(immediate, void, glMultiTexCoord3dv, (GLenum target, const GLdouble *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord3f, (GLenum target, GLfloat s, GLfloat t, GLfloat r), (target, s, t, r))
ORYON_SYM(immediate, void, glMultiTexCoord3fv, (GLenum target, const GLfloat *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord3i, (GLenum target, GLint s, GLint t, GLint r), (target, s, t, r))
ORYON_SYM(immediate, void, glMultiTexCoord3iv, (GLenum target, const GLint *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord3s, (GLenum target, GLshort s, GLshort t, GLshort r), (target, s, t, r))
ORYON_SYM(immediate, void, glMultiTexCoord3sv, (GLenum target, const GLshort *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord4d, (GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q), (target, s, t, r, q))
ORYON_SYM(immediate, void, glMultiTexCoord4dv, (GLenum target, const GLdouble *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord4f, (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q), (target, s, t, r, q))
ORYON_SYM(immediate, void, glMultiTexCoord4fv, (GLenum target, const GLfloat *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord4i, (GLenum target, GLint s, GLint t, GLint r, GLint q), (target, s, t, r, q))
ORYON_SYM(immediate, void, glMultiTexCoord4iv, (GLenum target, const GLint *v), (target, v))
ORYON_SYM(immediate, void, glMultiTexCoord4s, (GLenum target, GLshort s, GLshort t, GLshort r, GLshort q), (target, s, t, r, q))
ORYON_SYM(immediate, void, glMultiTexCoord4sv, (GLenum target, const GLshort *v), (target, v))

/* ---- displaylist ---- */
ORYON_SYM(displaylist, void, glNewList, (GLuint list, GLenum mode), (list, mode))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glNormal3f, (GLfloat nx, GLfloat ny, GLfloat nz), (nx, ny, nz))

/* ---- clientarr ---- */
ORYON_SYM(clientarr, void, glNormalPointer, (GLenum type, GLsizei stride, const GLvoid *ptr), (type, stride, ptr))

/* ---- matrix ---- */
ORYON_SYM(matrix, void, glOrtho, (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val), (left, right, bottom, top, near_val, far_val))

/* ---- texture ---- */
ORYON_SYM(texture, void, glPixelStorei, (GLenum pname, GLint param), (pname, param))

/* ---- translate ---- */
ORYON_SYM(translate, void, glPointParameterf, (GLenum pname, GLfloat param), (pname, param))
ORYON_SYM(translate, void, glPointParameterfv, (GLenum pname, const GLfloat *params), (pname, params))
ORYON_SYM(translate, void, glPointParameteri, (GLenum pname, GLint param), (pname, param))
ORYON_SYM(translate, void, glPointParameteriv, (GLenum pname, const GLint *params), (pname, params))

/* ---- legacy_misc ---- */
ORYON_SYM(legacy_misc, void, glPolygonMode, (GLenum face, GLenum mode), (face, mode))

/* ---- direct ---- */
ORYON_SYM(direct, void, glPolygonOffset, (GLfloat factor, GLfloat units), (factor, units))

/* ---- attribstack ---- */
ORYON_SYM(attribstack, void, glPopAttrib, (void), ())

/* ---- matrix ---- */
ORYON_SYM(matrix, void, glPopMatrix, (void), ())

/* ---- attribstack ---- */
ORYON_SYM(attribstack, void, glPushAttrib, (GLbitfield mask), (mask))

/* ---- matrix ---- */
ORYON_SYM(matrix, void, glPushMatrix, (void), ())

/* ---- texture ---- */
ORYON_SYM(texture, void, glReadPixels, (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels), (x, y, width, height, format, type, pixels))

/* ---- direct ---- */
ORYON_SYM(direct, void, glRenderbufferStorage, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height), (target, internalformat, width, height))

/* ---- alias ---- */
ORYON_SYM(alias, void, glRenderbufferStorageEXT, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height), (target, internalformat, width, height))

/* ---- direct ---- */
ORYON_SYM(direct, void, glRenderbufferStorageMultisample, (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height), (target, samples, internalformat, width, height))

/* ---- matrix ---- */
ORYON_SYM(matrix, void, glRotatef, (GLfloat angle, GLfloat x, GLfloat y, GLfloat z), (angle, x, y, z))

/* ---- direct ---- */
ORYON_SYM(direct, void, glSampleCoverage, (GLclampf value, GLboolean invert), (value, invert))

/* ---- matrix ---- */
ORYON_SYM(matrix, void, glScaled, (GLdouble x, GLdouble y, GLdouble z), (x, y, z))
ORYON_SYM(matrix, void, glScalef, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glSecondaryColor3b, (GLbyte red, GLbyte green, GLbyte blue), (red, green, blue))
ORYON_SYM(immediate, void, glSecondaryColor3bv, (const GLbyte *v), (v))
ORYON_SYM(immediate, void, glSecondaryColor3d, (GLdouble red, GLdouble green, GLdouble blue), (red, green, blue))
ORYON_SYM(immediate, void, glSecondaryColor3dv, (const GLdouble *v), (v))
ORYON_SYM(immediate, void, glSecondaryColor3f, (GLfloat red, GLfloat green, GLfloat blue), (red, green, blue))
ORYON_SYM(immediate, void, glSecondaryColor3fv, (const GLfloat *v), (v))
ORYON_SYM(immediate, void, glSecondaryColor3i, (GLint red, GLint green, GLint blue), (red, green, blue))
ORYON_SYM(immediate, void, glSecondaryColor3iv, (const GLint *v), (v))
ORYON_SYM(immediate, void, glSecondaryColor3s, (GLshort red, GLshort green, GLshort blue), (red, green, blue))
ORYON_SYM(immediate, void, glSecondaryColor3sv, (const GLshort *v), (v))
ORYON_SYM(immediate, void, glSecondaryColor3ub, (GLubyte red, GLubyte green, GLubyte blue), (red, green, blue))
ORYON_SYM(immediate, void, glSecondaryColor3ubv, (const GLubyte *v), (v))
ORYON_SYM(immediate, void, glSecondaryColor3ui, (GLuint red, GLuint green, GLuint blue), (red, green, blue))
ORYON_SYM(immediate, void, glSecondaryColor3uiv, (const GLuint *v), (v))
ORYON_SYM(immediate, void, glSecondaryColor3us, (GLushort red, GLushort green, GLushort blue), (red, green, blue))
ORYON_SYM(immediate, void, glSecondaryColor3usv, (const GLushort *v), (v))
ORYON_SYM(immediate, void, glSecondaryColorPointer, (GLint size, GLenum type, GLsizei stride, const void *pointer), (size, type, stride, pointer))

/* ---- lighting ---- */
ORYON_SYM(lighting, void, glShadeModel, (GLenum mode), (mode))

/* ---- shader ---- */
ORYON_SYM(shader, void, glShaderSource, (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length), (shader, count, string, length))

/* ---- alias ---- */
ORYON_SYM(alias, void, glShaderSourceARB, (GLhandleARB shaderObj, GLsizei count, const GLcharARB **string, const GLint *length), (shaderObj, count, string, length))

/* ---- direct ---- */
ORYON_SYM(direct, void, glStencilFuncSeparate, (GLenum face, GLenum func, GLint ref, GLuint mask), (face, func, ref, mask))
ORYON_SYM(direct, void, glStencilMaskSeparate, (GLenum face, GLuint mask), (face, mask))
ORYON_SYM(direct, void, glStencilOpSeparate, (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass), (face, sfail, dpfail, dppass))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glTexCoord2f, (GLfloat s, GLfloat t), (s, t))
ORYON_SYM(immediate, void, glTexCoordPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr), (size, type, stride, ptr))

/* ---- texenv ---- */
ORYON_SYM(texenv, void, glTexEnvf, (GLenum target, GLenum pname, GLfloat param), (target, pname, param))
ORYON_SYM(texenv, void, glTexEnvfv, (GLenum target, GLenum pname, const GLfloat *params), (target, pname, params))
ORYON_SYM(texenv, void, glTexEnvi, (GLenum target, GLenum pname, GLint param), (target, pname, param))
ORYON_SYM(texenv, void, glTexGenfv, (GLenum coord, GLenum pname, const GLfloat *params), (coord, pname, params))
ORYON_SYM(texenv, void, glTexGeni, (GLenum coord, GLenum pname, GLint param), (coord, pname, param))

/* ---- texture ---- */
ORYON_SYM(texture, void, glTexImage2D, (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels), (target, level, internalFormat, width, height, border, format, type, pixels))
ORYON_SYM(texture, void, glTexImage3D, (GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels), (target, level, internalFormat, width, height, depth, border, format, type, pixels))
ORYON_SYM(texture, void, glTexParameterf, (GLenum target, GLenum pname, GLfloat param), (target, pname, param))
ORYON_SYM(texture, void, glTexParameteri, (GLenum target, GLenum pname, GLint param), (target, pname, param))
ORYON_SYM(texture, void, glTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels), (target, level, xoffset, yoffset, width, height, format, type, pixels))
ORYON_SYM(texture, void, glTexSubImage3D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid *pixels), (target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels))

/* ---- matrix ---- */
ORYON_SYM(matrix, void, glTranslated, (GLdouble x, GLdouble y, GLdouble z), (x, y, z))
ORYON_SYM(matrix, void, glTranslatef, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniform1f, (GLint location, GLfloat v0), (location, v0))
ORYON_SYM(direct, void, glUniform1fv, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))

/* ---- alias ---- */
ORYON_SYM(alias, void, glUniform1fvARB, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniform1i, (GLint location, GLint v0), (location, v0))

/* ---- alias ---- */
ORYON_SYM(alias, void, glUniform1iARB, (GLint location, GLint v0), (location, v0))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniform1iv, (GLint location, GLsizei count, const GLint *value), (location, count, value))

/* ---- alias ---- */
ORYON_SYM(alias, void, glUniform1ivARB, (GLint location, GLsizei count, const GLint *value), (location, count, value))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniform2f, (GLint location, GLfloat v0, GLfloat v1), (location, v0, v1))
ORYON_SYM(direct, void, glUniform2fv, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))

/* ---- alias ---- */
ORYON_SYM(alias, void, glUniform2fvARB, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniform2i, (GLint location, GLint v0, GLint v1), (location, v0, v1))
ORYON_SYM(direct, void, glUniform2iv, (GLint location, GLsizei count, const GLint *value), (location, count, value))

/* ---- alias ---- */
ORYON_SYM(alias, void, glUniform2ivARB, (GLint location, GLsizei count, const GLint *value), (location, count, value))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniform3f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2), (location, v0, v1, v2))
ORYON_SYM(direct, void, glUniform3fv, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))

/* ---- alias ---- */
ORYON_SYM(alias, void, glUniform3fvARB, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniform3i, (GLint location, GLint v0, GLint v1, GLint v2), (location, v0, v1, v2))
ORYON_SYM(direct, void, glUniform3iv, (GLint location, GLsizei count, const GLint *value), (location, count, value))

/* ---- alias ---- */
ORYON_SYM(alias, void, glUniform3ivARB, (GLint location, GLsizei count, const GLint *value), (location, count, value))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniform4f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3), (location, v0, v1, v2, v3))
ORYON_SYM(direct, void, glUniform4fv, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))

/* ---- alias ---- */
ORYON_SYM(alias, void, glUniform4fvARB, (GLint location, GLsizei count, const GLfloat *value), (location, count, value))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniform4i, (GLint location, GLint v0, GLint v1, GLint v2, GLint v3), (location, v0, v1, v2, v3))
ORYON_SYM(direct, void, glUniform4iv, (GLint location, GLsizei count, const GLint *value), (location, count, value))

/* ---- alias ---- */
ORYON_SYM(alias, void, glUniform4ivARB, (GLint location, GLsizei count, const GLint *value), (location, count, value))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniformMatrix2fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value), (location, count, transpose, value))

/* ---- alias ---- */
ORYON_SYM(alias, void, glUniformMatrix2fvARB, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value), (location, count, transpose, value))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniformMatrix2x3fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value), (location, count, transpose, value))
ORYON_SYM(direct, void, glUniformMatrix2x4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value), (location, count, transpose, value))
ORYON_SYM(direct, void, glUniformMatrix3fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value), (location, count, transpose, value))

/* ---- alias ---- */
ORYON_SYM(alias, void, glUniformMatrix3fvARB, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value), (location, count, transpose, value))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniformMatrix3x2fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value), (location, count, transpose, value))
ORYON_SYM(direct, void, glUniformMatrix3x4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value), (location, count, transpose, value))
ORYON_SYM(direct, void, glUniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value), (location, count, transpose, value))

/* ---- alias ---- */
ORYON_SYM(alias, void, glUniformMatrix4fvARB, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value), (location, count, transpose, value))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUniformMatrix4x2fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value), (location, count, transpose, value))
ORYON_SYM(direct, void, glUniformMatrix4x3fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value), (location, count, transpose, value))

/* ---- buffer ---- */
ORYON_SYM(buffer, GLboolean, glUnmapBuffer, (GLenum target), (target))

/* ---- direct ---- */
ORYON_SYM(direct, void, glUseProgram, (GLuint program), (program))

/* ---- translate ---- */
ORYON_SYM(translate, void, glUseProgramObjectARB, (GLhandleARB programObj), (programObj))

/* ---- direct ---- */
ORYON_SYM(direct, void, glValidateProgram, (GLuint program), (program))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glVertex3f, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))
ORYON_SYM(immediate, void, glVertexAttrib1d, (GLuint index, GLdouble x), (index, x))
ORYON_SYM(immediate, void, glVertexAttrib1dv, (GLuint index, const GLdouble *v), (index, v))

/* ---- direct ---- */
ORYON_SYM(direct, void, glVertexAttrib1f, (GLuint index, GLfloat x), (index, x))
ORYON_SYM(direct, void, glVertexAttrib1fv, (GLuint index, const GLfloat *v), (index, v))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glVertexAttrib1s, (GLuint index, GLshort x), (index, x))
ORYON_SYM(immediate, void, glVertexAttrib1sv, (GLuint index, const GLshort *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib2d, (GLuint index, GLdouble x, GLdouble y), (index, x, y))
ORYON_SYM(immediate, void, glVertexAttrib2dv, (GLuint index, const GLdouble *v), (index, v))

/* ---- direct ---- */
ORYON_SYM(direct, void, glVertexAttrib2f, (GLuint index, GLfloat x, GLfloat y), (index, x, y))
ORYON_SYM(direct, void, glVertexAttrib2fv, (GLuint index, const GLfloat *v), (index, v))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glVertexAttrib2s, (GLuint index, GLshort x, GLshort y), (index, x, y))
ORYON_SYM(immediate, void, glVertexAttrib2sv, (GLuint index, const GLshort *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib3d, (GLuint index, GLdouble x, GLdouble y, GLdouble z), (index, x, y, z))
ORYON_SYM(immediate, void, glVertexAttrib3dv, (GLuint index, const GLdouble *v), (index, v))

/* ---- direct ---- */
ORYON_SYM(direct, void, glVertexAttrib3f, (GLuint index, GLfloat x, GLfloat y, GLfloat z), (index, x, y, z))
ORYON_SYM(direct, void, glVertexAttrib3fv, (GLuint index, const GLfloat *v), (index, v))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glVertexAttrib3s, (GLuint index, GLshort x, GLshort y, GLshort z), (index, x, y, z))
ORYON_SYM(immediate, void, glVertexAttrib3sv, (GLuint index, const GLshort *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib4Nbv, (GLuint index, const GLbyte *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib4Niv, (GLuint index, const GLint *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib4Nsv, (GLuint index, const GLshort *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib4Nub, (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w), (index, x, y, z, w))
ORYON_SYM(immediate, void, glVertexAttrib4Nubv, (GLuint index, const GLubyte *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib4Nuiv, (GLuint index, const GLuint *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib4Nusv, (GLuint index, const GLushort *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib4bv, (GLuint index, const GLbyte *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib4d, (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w), (index, x, y, z, w))
ORYON_SYM(immediate, void, glVertexAttrib4dv, (GLuint index, const GLdouble *v), (index, v))

/* ---- direct ---- */
ORYON_SYM(direct, void, glVertexAttrib4f, (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w), (index, x, y, z, w))
ORYON_SYM(direct, void, glVertexAttrib4fv, (GLuint index, const GLfloat *v), (index, v))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glVertexAttrib4iv, (GLuint index, const GLint *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib4s, (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w), (index, x, y, z, w))
ORYON_SYM(immediate, void, glVertexAttrib4sv, (GLuint index, const GLshort *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib4ubv, (GLuint index, const GLubyte *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib4uiv, (GLuint index, const GLuint *v), (index, v))
ORYON_SYM(immediate, void, glVertexAttrib4usv, (GLuint index, const GLushort *v), (index, v))

/* ---- direct ---- */
ORYON_SYM(direct, void, glVertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer), (index, size, type, normalized, stride, pointer))

/* ---- immediate ---- */
ORYON_SYM(immediate, void, glVertexPointer, (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr), (size, type, stride, ptr))

/* ---- direct ---- */
ORYON_SYM(direct, void, glViewport, (GLint x, GLint y, GLsizei width, GLsizei height), (x, y, width, height))

/* ---- legacy_misc ---- */
ORYON_SYM(legacy_misc, void, glWindowPos2d, (GLdouble x, GLdouble y), (x, y))
ORYON_SYM(legacy_misc, void, glWindowPos2dv, (const GLdouble *v), (v))
ORYON_SYM(legacy_misc, void, glWindowPos2f, (GLfloat x, GLfloat y), (x, y))
ORYON_SYM(legacy_misc, void, glWindowPos2fv, (const GLfloat *v), (v))
ORYON_SYM(legacy_misc, void, glWindowPos2i, (GLint x, GLint y), (x, y))
ORYON_SYM(legacy_misc, void, glWindowPos2iv, (const GLint *v), (v))
ORYON_SYM(legacy_misc, void, glWindowPos2s, (GLshort x, GLshort y), (x, y))
ORYON_SYM(legacy_misc, void, glWindowPos2sv, (const GLshort *v), (v))
ORYON_SYM(legacy_misc, void, glWindowPos3d, (GLdouble x, GLdouble y, GLdouble z), (x, y, z))
ORYON_SYM(legacy_misc, void, glWindowPos3dv, (const GLdouble *v), (v))
ORYON_SYM(legacy_misc, void, glWindowPos3f, (GLfloat x, GLfloat y, GLfloat z), (x, y, z))
ORYON_SYM(legacy_misc, void, glWindowPos3fv, (const GLfloat *v), (v))
ORYON_SYM(legacy_misc, void, glWindowPos3i, (GLint x, GLint y, GLint z), (x, y, z))
ORYON_SYM(legacy_misc, void, glWindowPos3iv, (const GLint *v), (v))
ORYON_SYM(legacy_misc, void, glWindowPos3s, (GLshort x, GLshort y, GLshort z), (x, y, z))
ORYON_SYM(legacy_misc, void, glWindowPos3sv, (const GLshort *v), (v))

#undef ORYON_SYM
