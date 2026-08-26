# Permukaan Simbol Oryon

Dihasilkan dari ground-truth, bukan tebakan:

| sumber | isi |
|---|---|
| `1.12.2.jar` | call site GL asli Minecraft |
| `lwjglglfwclasses.jar` | 2225 simbol kanonik + tuntutan tiap flag kapabilitas |
| `/usr/include/GL/*.h` | prototipe C yang tepat |
| `/usr/include/GLES*/*.h` | permukaan GLES 3.2 nyata |

## Profil

- `GL_VERSION` dilaporkan: **2.1 Oryon**
- `GL_SHADING_LANGUAGE_VERSION`: **1.20**
- Flag yang dibuat TRUE: `GL12`, `GL13`, `GL14`, `GL15`, `GL20`, `GL21`, `ARB_framebuffer_object`

## Angka

| | jumlah |
|---|---|
| Total ekspor | **352** |
| Dipanggil langsung oleh 1.12.2 | 163 |
| Pelengkap agar flag kapabilitas TRUE | 189 |

## Strategi

| strategi | n | arti |
|---|---|---|
| `direct` | 117 | Identik di GLES 3.2. Panggilan lewat penunjuk fungsi driver, tanpa lapisan. |
| `immediate` | 92 | Mode langsung glBegin/glEnd. Dirakit ke buffer vertex, dikirim sekali per glEnd. |
| `alias` | 33 | Nama ber-akhiran ARB/EXT yang setara persis dengan nama inti GLES. |
| `translate` | 19 | Ada padanannya, butuh konversi tipis (tipe, urutan argumen, atau emulasi kecil). |
| `legacy_misc` | 19 | Sisa warisan tanpa padanan GLES. Sebagian besar tidak pernah dipanggil 1.12.2. |
| `matrix` | 15 | Tumpukan matriks fixed-function. Dihitung di CPU, dikirim sebagai uniform. |
| `texture` | 13 |  |
| `buffer` | 7 |  |
| `clientarr` | 6 | Array sisi klien fixed-function. Dipetakan ke VAO + atribut vertex generik. |
| `query` | 5 |  |
| `texenv` | 5 | Lingkungan tekstur / pembangkit koordinat. Menjadi bagian kunci state shader. |
| `displaylist` | 4 | Display list. Direkam sebagai daftar perintah, diputar ulang saat glCallList. |
| `lighting` | 4 | Pencahayaan fixed-function. Menjadi bagian kunci state generator shader. |
| `draw` | 4 |  |
| `fog` | 3 | Kabut fixed-function. Menjadi bagian kunci state generator shader. |
| `capbits` | 2 |  |
| `attribstack` | 2 | glPushAttrib/glPopAttrib. Simpan/pulihkan potongan state yang dibutuhkan saja. |
| `alphatest` | 1 | Uji alpha. Menjadi cabang discard di fragment shader. |
| `shader` | 1 |  |

## Daftar per strategi

<details><summary><b>direct</b> (117)</summary>

```
glAttachShader glBeginQuery glBindAttribLocation glBindFramebuffer glBindRenderbuffer 
glBlendColor glBlendEquation glBlendEquationSeparate glBlendFunc glBlendFuncSeparate 
glBlitFramebuffer glCheckFramebufferStatus glClear glClearColor glColorMask 
glCompileShader glCompressedTexImage3D glCompressedTexSubImage3D glCopyTexSubImage3D 
glCreateProgram glCreateShader glCullFace glDeleteBuffers glDeleteFramebuffers 
glDeleteProgram glDeleteQueries glDeleteRenderbuffers glDeleteShader glDeleteTextures 
glDepthFunc glDepthMask glDetachShader glDisableVertexAttribArray glDrawBuffers 
glEnableVertexAttribArray glEndQuery glFramebufferRenderbuffer glFramebufferTexture2D 
glFramebufferTextureLayer glGenBuffers glGenFramebuffers glGenQueries glGenRenderbuffers 
glGenTextures glGenerateMipmap glGetActiveAttrib glGetActiveUniform glGetAttachedShaders 
glGetAttribLocation glGetBufferPointerv glGetFramebufferAttachmentParameteriv 
glGetProgramInfoLog glGetProgramiv glGetQueryObjectuiv glGetQueryiv 
glGetRenderbufferParameteriv glGetShaderInfoLog glGetShaderSource glGetShaderiv 
glGetUniformLocation glGetUniformfv glGetUniformiv glGetVertexAttribPointerv 
glGetVertexAttribfv glGetVertexAttribiv glIsBuffer glIsFramebuffer glIsProgram glIsQuery 
glIsRenderbuffer glIsShader glLineWidth glLinkProgram glPolygonOffset 
glRenderbufferStorage glRenderbufferStorageMultisample glSampleCoverage 
glStencilFuncSeparate glStencilMaskSeparate glStencilOpSeparate glUniform1f glUniform1fv 
glUniform1i glUniform1iv glUniform2f glUniform2fv glUniform2i glUniform2iv glUniform3f 
glUniform3fv glUniform3i glUniform3iv glUniform4f glUniform4fv glUniform4i glUniform4iv 
glUniformMatrix2fv glUniformMatrix2x3fv glUniformMatrix2x4fv glUniformMatrix3fv 
glUniformMatrix3x2fv glUniformMatrix3x4fv glUniformMatrix4fv glUniformMatrix4x2fv 
glUniformMatrix4x3fv glUseProgram glValidateProgram glVertexAttrib1f glVertexAttrib1fv 
glVertexAttrib2f glVertexAttrib2fv glVertexAttrib3f glVertexAttrib3fv glVertexAttrib4f 
glVertexAttrib4fv glVertexAttribPointer glViewport 
```

</details>

<details><summary><b>immediate</b> (92)</summary>

```
glBegin glColor4f glEnd glEndList glFogCoordPointer glFogCoordd glFogCoorddv glFogCoordf 
glFogCoordfv glMultiTexCoord1d glMultiTexCoord1dv glMultiTexCoord1f glMultiTexCoord1fv 
glMultiTexCoord1i glMultiTexCoord1iv glMultiTexCoord1s glMultiTexCoord1sv 
glMultiTexCoord2d glMultiTexCoord2dv glMultiTexCoord2f glMultiTexCoord2fARB 
glMultiTexCoord2fv glMultiTexCoord2i glMultiTexCoord2iv glMultiTexCoord2s 
glMultiTexCoord2sv glMultiTexCoord3d glMultiTexCoord3dv glMultiTexCoord3f 
glMultiTexCoord3fv glMultiTexCoord3i glMultiTexCoord3iv glMultiTexCoord3s 
glMultiTexCoord3sv glMultiTexCoord4d glMultiTexCoord4dv glMultiTexCoord4f 
glMultiTexCoord4fv glMultiTexCoord4i glMultiTexCoord4iv glMultiTexCoord4s 
glMultiTexCoord4sv glNormal3f glSecondaryColor3b glSecondaryColor3bv glSecondaryColor3d 
glSecondaryColor3dv glSecondaryColor3f glSecondaryColor3fv glSecondaryColor3i 
glSecondaryColor3iv glSecondaryColor3s glSecondaryColor3sv glSecondaryColor3ub 
glSecondaryColor3ubv glSecondaryColor3ui glSecondaryColor3uiv glSecondaryColor3us 
glSecondaryColor3usv glSecondaryColorPointer glTexCoord2f glTexCoordPointer glVertex3f 
glVertexAttrib1d glVertexAttrib1dv glVertexAttrib1s glVertexAttrib1sv glVertexAttrib2d 
glVertexAttrib2dv glVertexAttrib2s glVertexAttrib2sv glVertexAttrib3d glVertexAttrib3dv 
glVertexAttrib3s glVertexAttrib3sv glVertexAttrib4Nbv glVertexAttrib4Niv 
glVertexAttrib4Nsv glVertexAttrib4Nub glVertexAttrib4Nubv glVertexAttrib4Nuiv 
glVertexAttrib4Nusv glVertexAttrib4bv glVertexAttrib4d glVertexAttrib4dv 
glVertexAttrib4iv glVertexAttrib4s glVertexAttrib4sv glVertexAttrib4ubv 
glVertexAttrib4uiv glVertexAttrib4usv glVertexPointer 
```

</details>

<details><summary><b>alias</b> (33)</summary>

```
glActiveTextureARB glBindBufferARB glBindFramebufferEXT glBindRenderbufferEXT 
glBlendFuncSeparateEXT glBufferDataARB glCheckFramebufferStatusEXT glCompileShaderARB 
glDeleteBuffersARB glDeleteFramebuffersEXT glDeleteRenderbuffersEXT 
glFramebufferRenderbufferEXT glFramebufferTexture2DEXT glGenBuffersARB 
glGenFramebuffersEXT glGenRenderbuffersEXT glGetAttribLocationARB 
glGetUniformLocationARB glLinkProgramARB glRenderbufferStorageEXT glShaderSourceARB 
glUniform1fvARB glUniform1iARB glUniform1ivARB glUniform2fvARB glUniform2ivARB 
glUniform3fvARB glUniform3ivARB glUniform4fvARB glUniform4ivARB glUniformMatrix2fvARB 
glUniformMatrix3fvARB glUniformMatrix4fvARB 
```

</details>

<details><summary><b>translate</b> (19)</summary>

```
glAttachObjectARB glClearDepth glCompressedTexImage1D glCompressedTexSubImage1D 
glCreateProgramObjectARB glCreateShaderObjectARB glDeleteObjectARB 
glFramebufferTexture1D glFramebufferTexture3D glGetCompressedTexImage glGetInfoLogARB 
glGetObjectParameterivARB glGetQueryObjectiv glGetVertexAttribdv glPointParameterf 
glPointParameterfv glPointParameteri glPointParameteriv glUseProgramObjectARB 
```

</details>

<details><summary><b>legacy_misc</b> (19)</summary>

```
glGetTexImage glLogicOp glPolygonMode glWindowPos2d glWindowPos2dv glWindowPos2f 
glWindowPos2fv glWindowPos2i glWindowPos2iv glWindowPos2s glWindowPos2sv glWindowPos3d 
glWindowPos3dv glWindowPos3f glWindowPos3fv glWindowPos3i glWindowPos3iv glWindowPos3s 
glWindowPos3sv 
```

</details>

<details><summary><b>matrix</b> (15)</summary>

```
glLoadIdentity glLoadTransposeMatrixd glLoadTransposeMatrixf glMatrixMode glMultMatrixf 
glMultTransposeMatrixd glMultTransposeMatrixf glOrtho glPopMatrix glPushMatrix glRotatef 
glScaled glScalef glTranslated glTranslatef 
```

</details>

<details><summary><b>texture</b> (13)</summary>

```
glActiveTexture glBindTexture glCompressedTexImage2D glCompressedTexSubImage2D 
glCopyTexSubImage2D glPixelStorei glReadPixels glTexImage2D glTexImage3D glTexParameterf 
glTexParameteri glTexSubImage2D glTexSubImage3D 
```

</details>

<details><summary><b>buffer</b> (7)</summary>

```
glBindBuffer glBufferData glBufferSubData glGetBufferParameteriv glGetBufferSubData 
glMapBuffer glUnmapBuffer 
```

</details>

<details><summary><b>clientarr</b> (6)</summary>

```
glClientActiveTexture glClientActiveTextureARB glColorPointer glDisableClientState 
glEnableClientState glNormalPointer 
```

</details>

<details><summary><b>query</b> (5)</summary>

```
glGetError glGetFloatv glGetIntegerv glGetString glGetTexLevelParameteriv 
```

</details>

<details><summary><b>texenv</b> (5)</summary>

```
glTexEnvf glTexEnvfv glTexEnvi glTexGenfv glTexGeni 
```

</details>

<details><summary><b>displaylist</b> (4)</summary>

```
glCallList glDeleteLists glGenLists glNewList 
```

</details>

<details><summary><b>lighting</b> (4)</summary>

```
glColorMaterial glLightModelfv glLightfv glShadeModel 
```

</details>

<details><summary><b>draw</b> (4)</summary>

```
glDrawArrays glDrawRangeElements glMultiDrawArrays glMultiDrawElements 
```

</details>

<details><summary><b>fog</b> (3)</summary>

```
glFogf glFogfv glFogi 
```

</details>

<details><summary><b>capbits</b> (2)</summary>

```
glDisable glEnable 
```

</details>

<details><summary><b>attribstack</b> (2)</summary>

```
glPopAttrib glPushAttrib 
```

</details>

<details><summary><b>alphatest</b> (1)</summary>

```
glAlphaFunc 
```

</details>

<details><summary><b>shader</b> (1)</summary>

```
glShaderSource 
```

</details>
