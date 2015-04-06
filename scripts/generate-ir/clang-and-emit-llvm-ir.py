#!/usr/bin/env python3

import sys
import subprocess
import os
import argparse
from termcolor import colored, cprint
from enum import Enum

from linkerwrapper import *

# TODO: always build with optimizations off (does that include DCE?) or maybe -fno-inline is better?

# TODO: do we need to wrap objcopy?

# TODO: there is also an llvm-ar command, will those archives also work with llvm-link?
# or is it maybe better to use that instead of llvm-link?

# TODO: heuristics to skip LLVM IR generation in cmake or autoconf configure checks?

# TODO: handle -S flag (assembly generation): Do any build systems even use this?

# TODO: libtool messes up everything, work around that

# returns (outputStr, outputIndex) tuple
def getOutputParam(cmdline):
    outputIdx = -1
    output = None
    if '-o' in cmdline:
        outputIdx = cmdline.index('-o') + 1
        if outputIdx >= len(cmdline):
            sys.stderr.write('WARNING: -o flag given but no parameter to it!')
        else:
            output = correspondingBitcodeName(cmdline[outputIdx])
    return (output, outputIdx)


# MAIN script:

# why is this not in the stdlib...
def strip_end(text, suffix):
    text = str(text)
    if not text.endswith(suffix):
        return text
    return text[:len(text) - len(suffix)]

executable = strip_end(os.path.basename(sys.argv[0]), "-and-emit-llvm-ir.py")
sys.argv[0] = executable

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
    output, outputIdx = getOutputParam(compile_cmdline)
    if "-c" in compile_cmdline:
        wrapper = CompilerWrapper(sys.argv)
    else:
       wrapper = LinkerWrapper(sys.argv)

else:
    raise(RuntimeError('Could not parse command line to determine what\'s happening: ', sys.argv)

wrapper.run()
