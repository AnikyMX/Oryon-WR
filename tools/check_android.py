#!/usr/bin/env python3
# tools/check_android.py -- compile every source for aarch64-linux-android24 against real bionic libc headers
# (-fsyntax-only), mirroring the CI flags. Catches glibc-vs-bionic differences (signals, ucontext, link.h, syscalls)
# without the NDK. Needs: ORYON_BIONIC = checkout of AOSP bionic (libc/include, libc/kernel/uapi, libc/kernel/android),
# ORYON_KHR_INC = directory with EGL/, GLES2/, GLES3/, KHR/ Khronos headers.
import os, sys, glob, subprocess, tempfile
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
B = os.environ.get('ORYON_BIONIC'); K = os.environ.get('ORYON_KHR_INC', '/usr/include')
if not B or not os.path.isdir(os.path.join(B, 'libc', 'include')):
    print('check_android: set ORYON_BIONIC to a bionic checkout'); sys.exit(2)
res = subprocess.run(['clang++', '-print-resource-dir'], capture_output=True, text=True).stdout.strip()
stub = tempfile.mkdtemp(prefix='oryon-ndkstub-')
os.makedirs(os.path.join(stub, 'android'))
open(os.path.join(stub, 'android', 'log.h'), 'w').write(
    '#pragma once\n#define ANDROID_LOG_INFO 4\n#ifdef __cplusplus\nextern "C"\n#endif\nint __android_log_write(int prio, const char *tag, const char *text);\n')
khr = tempfile.mkdtemp(prefix='oryon-khr-')
for d in ('EGL', 'GLES2', 'GLES3', 'KHR'):
    os.symlink(os.path.join(K, d), os.path.join(khr, d))
inc = ['-isystem', os.path.join(res, 'include'), '-isystem', os.path.join(B, 'libc', 'include'),
       '-isystem', os.path.join(B, 'libc', 'kernel', 'uapi', 'asm-arm64'), '-isystem', os.path.join(B, 'libc', 'kernel', 'uapi'),
       '-isystem', os.path.join(B, 'libc', 'kernel', 'android', 'uapi'), '-isystem', stub, '-isystem', khr]
flags = ['--target=aarch64-linux-android24', '-std=c++17', '-nostdinc', '-nostdinc++', '-fsyntax-only', '-O2',
         '-fvisibility=hidden', '-fno-exceptions', '-fno-rtti', '-fno-threadsafe-statics', '-Wall', '-Wextra',
         '-Wno-unused-parameter', '-Werror', '-DORYON_VERSION="0.1.0"', '-I', os.path.join(ROOT, 'src')]
srcs = sorted(glob.glob(os.path.join(ROOT, 'src', '*.cpp')) + glob.glob(os.path.join(ROOT, 'src', 'gen', '*.cpp')))
bad = 0
for s in srcs:
    p = subprocess.run(['clang++'] + flags + inc + [s], capture_output=True, text=True)
    if p.returncode:
        bad += 1; print('FAIL', os.path.relpath(s, ROOT)); print(p.stderr[-3000:])
print('check_android: %d sources, %d failed (aarch64-linux-android24, bionic headers, -Werror)' % (len(srcs), bad))
sys.exit(1 if bad else 0)
