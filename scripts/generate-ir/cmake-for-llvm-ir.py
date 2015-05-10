#! /bin/sh
# This file is used as both a shell script and as a Python script.
""":"
# This part is run by the shell.  It looks for an appropriate Python
# interpreter then uses it to re-exec this script.

if test `which python3`
then
  echo "Py3 found"
  PYTHON=`which python3`
elif test `which python2`
then
  PYTHON=`which python2`
else
  echo 1>&2 "No usable Python interpreter was found!"
  exit 1
fi

exec $PYTHON "$0" "$@"
" """

import sys
import os

scriptDir = os.path.dirname(os.path.realpath(__file__))


def irWrapper(var, command):
    wrapper = os.path.join(scriptDir, command + '-and-emit-llvm-ir.py')
    if not os.path.exists(wrapper):
        sys.exit('could not find ' + wrapper)
    return '-DCMAKE_' + var + '=' + wrapper


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
os.environ['NO_EMIT_LLVM_IR'] = '1'
# no need for subprocess.call, just use execvpe
os.execvpe(commandline[0], commandline, os.environ)
sys.exit('Could not execute ' + str(commandline))
