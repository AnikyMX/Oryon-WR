# tools/khr.py -- parse Khronos EGL/GLES headers (source of truth for the ES side)
import re, os
INC = os.environ.get('ORYON_KHR_INC', '/usr/include')
_DEF = re.compile(r'^\s*#define\s+((?:GL|EGL)_\w+)\s+(\(?\s*(?:\(\w+\)\s*)?-?(?:0x[0-9A-Fa-f]+|\d+)[uUlL]*\s*\)?)\s*$')
def parse_defines(*paths):
    out = {}
    for p in paths:
        for ln in open(os.path.join(INC, p), encoding='utf-8', errors='replace'):
            m = _DEF.match(ln)
            if not m: continue
            v = re.sub(r'[()\s]|\(\w+\)|[uUlL]+$', '', m.group(2))
            v = re.sub(r'^\(?\w+\)', '', v) if not re.match(r'^-?(0x|\d)', v) else v
            try: out.setdefault(m.group(1), int(v, 0))
            except ValueError: pass
    return out
_PROTO = re.compile(r'^\s*(?:GL_APICALL|EGLAPI)\s+(.+?)\s*(?:GL_APIENTRY|EGLAPIENTRY)\s+(\w+)\s*\((.*?)\)\s*;')
def parse_protos(*paths):
    out = {}
    for p in paths:
        for ln in open(os.path.join(INC, p), encoding='utf-8', errors='replace'):
            m = _PROTO.match(ln)
            if not m: continue
            args = m.group(3).strip()
            al = [] if args in ('', 'void') else [a.strip() for a in args.split(',')]
            out.setdefault(m.group(2), (m.group(1).strip(), al))
    return out
def gles32():
    return parse_protos('GLES3/gl3.h', 'GLES3/gl31.h', 'GLES3/gl32.h')
def gles_ext():
    return parse_protos('GLES2/gl2ext.h')
def gl_defines():
    return parse_defines('GLES3/gl32.h', 'GLES2/gl2ext.h')
def egl_defines():
    return parse_defines('EGL/egl.h', 'EGL/eglext.h')
