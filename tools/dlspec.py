# tools/dlspec.py -- commands compiled into display lists (GL 1.x-3.0 compatibility semantics).
# Scalar-argument recorders are generated (tools/gen.py); pointer-argument recorders live in src/dlist.cpp.
DL_SCALAR = [
    'glEnable', 'glDisable', 'glBindTexture', 'glBlendFunc', 'glBlendFuncSeparate', 'glBlendEquation',
    'glBlendEquationSeparate', 'glBlendColor', 'glAlphaFunc', 'glDepthFunc', 'glDepthMask', 'glDepthRange',
    'glColorMask', 'glCullFace', 'glFrontFace', 'glShadeModel', 'glLineWidth', 'glPointSize', 'glPolygonOffset',
    'glPolygonMode', 'glScissor', 'glViewport', 'glStencilFunc', 'glStencilOp', 'glStencilMask',
    'glStencilFuncSeparate', 'glStencilOpSeparate', 'glStencilMaskSeparate', 'glClearColor', 'glClearDepth',
    'glClearStencil', 'glClear', 'glLogicOp', 'glHint', 'glMatrixMode', 'glLoadIdentity', 'glTranslatef',
    'glTranslated', 'glRotatef', 'glRotated', 'glScalef', 'glScaled', 'glOrtho', 'glFrustum', 'glPushMatrix',
    'glPopMatrix', 'glPushAttrib', 'glPopAttrib', 'glActiveTexture', 'glTexEnvf', 'glTexEnvi', 'glTexGeni',
    'glTexGenf', 'glTexGend', 'glTexParameteri', 'glTexParameterf', 'glFogf', 'glFogi', 'glLightf', 'glLighti',
    'glLightModelf', 'glLightModeli', 'glMaterialf', 'glMateriali', 'glColorMaterial', 'glSampleCoverage',
]
DL_POINTER = [
    'glLightfv', 'glLightiv', 'glLightModelfv', 'glLightModeliv', 'glMaterialfv', 'glMaterialiv', 'glFogfv', 'glFogiv',
    'glTexEnvfv', 'glTexEnviv', 'glTexGenfv', 'glTexGeniv', 'glTexGendv', 'glTexParameterfv', 'glTexParameteriv',
    'glLoadMatrixf', 'glLoadMatrixd', 'glMultMatrixf', 'glMultMatrixd', 'glLoadTransposeMatrixf',
    'glLoadTransposeMatrixd', 'glMultTransposeMatrixf', 'glMultTransposeMatrixd', 'glClipPlane',
]
