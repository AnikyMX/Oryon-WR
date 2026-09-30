# tools/eglctx.py -- Mesa EGL + GLES 3.2 context via ctypes (dlopen/dlsym), for real-driver tests
import ctypes, ctypes.util, os, sys
sys.path.insert(0, os.path.dirname(__file__))
import khr
E = khr.egl_defines()
_egl = ctypes.CDLL('libEGL.so.1', mode=ctypes.RTLD_GLOBAL)
_P = ctypes.c_void_p
def _f(name, res, *args):
    fn = getattr(_egl, name); fn.restype = res; fn.argtypes = list(args); return fn
eglGetProcAddress = _f('eglGetProcAddress', _P, ctypes.c_char_p)
eglInitialize = _f('eglInitialize', ctypes.c_uint, _P, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_int))
eglBindAPI = _f('eglBindAPI', ctypes.c_uint, ctypes.c_uint)
eglChooseConfig = _f('eglChooseConfig', ctypes.c_uint, _P, ctypes.POINTER(ctypes.c_int), ctypes.POINTER(_P), ctypes.c_int, ctypes.POINTER(ctypes.c_int))
eglCreateContext = _f('eglCreateContext', _P, _P, _P, _P, ctypes.POINTER(ctypes.c_int))
eglCreatePbufferSurface = _f('eglCreatePbufferSurface', _P, _P, _P, ctypes.POINTER(ctypes.c_int))
eglMakeCurrent = _f('eglMakeCurrent', ctypes.c_uint, _P, _P, _P, _P)
eglGetError = _f('eglGetError', ctypes.c_int)
eglQueryString = _f('eglQueryString', ctypes.c_char_p, _P, ctypes.c_int)
eglSwapBuffers = _f('eglSwapBuffers', ctypes.c_uint, _P, _P)
eglTerminate = _f('eglTerminate', ctypes.c_uint, _P)
def _ia(*v): return (ctypes.c_int * (len(v) + 1))(*v, E['EGL_NONE'])
class Context:
    def __init__(self, w=256, h=256, major=3, minor=2):
        gpd = ctypes.CFUNCTYPE(_P, ctypes.c_uint, _P, _P)(eglGetProcAddress(b'eglGetPlatformDisplayEXT'))
        self.dpy = gpd(E['EGL_PLATFORM_SURFACELESS_MESA'], None, None)
        a, b = ctypes.c_int(), ctypes.c_int()
        if not eglInitialize(self.dpy, ctypes.byref(a), ctypes.byref(b)): raise RuntimeError('eglInitialize %#x' % eglGetError())
        self.egl_version = (a.value, b.value)
        eglBindAPI(E['EGL_OPENGL_ES_API'])
        cfg = _P(); n = ctypes.c_int()
        attrs = _ia(E['EGL_SURFACE_TYPE'], E['EGL_PBUFFER_BIT'], E['EGL_RENDERABLE_TYPE'], E['EGL_OPENGL_ES3_BIT'],
                    E['EGL_RED_SIZE'], 8, E['EGL_GREEN_SIZE'], 8, E['EGL_BLUE_SIZE'], 8, E['EGL_ALPHA_SIZE'], 8,
                    E['EGL_DEPTH_SIZE'], 24, E['EGL_STENCIL_SIZE'], 8)
        if not eglChooseConfig(self.dpy, attrs, ctypes.byref(cfg), 1, ctypes.byref(n)) or n.value < 1:
            raise RuntimeError('eglChooseConfig %#x' % eglGetError())
        self.surf = eglCreatePbufferSurface(self.dpy, cfg, _ia(E['EGL_WIDTH'], w, E['EGL_HEIGHT'], h))
        self.ctx = eglCreateContext(self.dpy, cfg, None, _ia(E['EGL_CONTEXT_MAJOR_VERSION'], major, E['EGL_CONTEXT_MINOR_VERSION'], minor))
        if not self.ctx: raise RuntimeError('eglCreateContext %#x' % eglGetError())
        if not eglMakeCurrent(self.dpy, self.surf, self.surf, self.ctx): raise RuntimeError('eglMakeCurrent %#x' % eglGetError())
        self.w, self.h = w, h
    def swap(self): eglSwapBuffers(self.dpy, self.surf)
    def close(self):
        eglMakeCurrent(self.dpy, None, None, None); eglTerminate(self.dpy)
