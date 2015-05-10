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
import subprocess
import os
import argparse
from termcolor import colored, cprint
from enum import Enum

from linkerwrapper import *
from compilerwrapper import CompilerWrapper

# TODO: always build with optimizations off (does that include DCE?) or maybe -fno-inline is better?

# TODO: do we need to wrap objcopy?

# TODO: there is also an llvm-ar command, will those archives also work with llvm-link?
# or is it maybe better to use that instead of llvm-link?

# TODO: heuristics to skip LLVM IR generation in cmake or autoconf configure checks?

# TODO: handle -S flag (assembly generation): Do any build systems even use this?

# TODO: libtool messes up everything, work around that


# why is this not in the stdlib...
def strip_end(text, suffix):
    text = str(text)
    if not text.endswith(suffix):
        return text
    return text[:len(text) - len(suffix)]

executable = strip_end(os.path.basename(sys.argv[0]), "-and-emit-llvm-ir.py")
sys.argv[0] = executable

wrapper = None
# ar is used to add together .o files to a static library
if executable == 'ar':
    wrapper = ArWrapper(sys.argv)
# ranlib creates an index in the .a file -> we can skip this since we have created a llvm bitcode file
elif executable == 'ranlib':
    wrapper = RanlibWrapper(sys.argv)
# direct invocation of the linker: can be ld, gold or lld
elif executable in ('ld', 'lld', 'gold'):
    wrapper = LinkerWrapper(sys.argv)
elif executable in ('clang', 'clang++'):
    if '-c' in sys.argv:
        wrapper = CompilerWrapper(sys.argv)
    elif '-S' in sys.argv:
        raise RuntimeError('assembly generation not supported!', sys.argv)
    else:
        wrapper = LinkerWrapper(sys.argv)
else:
    raise RuntimeError('Could not parse command line to determine what\'s happening: ', sys.argv)

wrapper.run()
