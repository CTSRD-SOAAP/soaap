#!/usr/bin/env python3

import sys
import os
import subprocess

scriptDir = os.path.dirname(os.path.realpath(__file__))


def irWrapper(var, command):
    wrapper = os.path.join(scriptDir, command + '-and-emit-llvm-ir.py');
    if not os.path.exists(wrapper):
        sys.exit('could not find ' + wrapper)
    return '"-DCMAKE_' + var + '=' + wrapper + '"'


commandline = [
    'cmake',
    irWrapper('C_COMPILER', 'clang'),
    irWrapper('CXX_COMPILER', 'clang++'),
    irWrapper('LINKER', 'ld'),
    irWrapper('AR', 'ar'),
    irWrapper('RANLIB', 'ranlib')
]
hasGenerator = any(elem.startswith('-G') for elem in sys.argv)
if not hasGenerator:
    commandline.append('-GNinja')
    # commandline.append('-DCMAKE_MAKE_PROGRAM=/usr/bin/ninja')
    # WTF is happening here:
    # CMake Error: Error required internal CMake variable not set, cmake may be not be built correctly.
    # Missing variable is:
    # CMAKE_MAKE_PROGRAM
commandline.extend(sys.argv[1:])  # append all the user passed flags

print(commandline)
subprocess.call(commandline)
