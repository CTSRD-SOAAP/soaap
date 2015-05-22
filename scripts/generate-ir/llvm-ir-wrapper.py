#!/usr/bin/env python3

import sys
import os

from linkerwrapper import *
from compilerwrapper import CompilerWrapper
from unixcommandswrapper import *

# TODO: do we need to wrap objcopy?

executable = os.path.basename(sys.argv[0])
if executable in ('gcc', 'cc'):
    executable = 'clang'
if executable == ('g++', 'c++'):
    executable = 'clang++'
sys.argv[0] = executable

wrapper = None
if executable in ('clang', 'clang++'):
    if '-c' in sys.argv or '-S' in sys.argv:
        wrapper = CompilerWrapper(sys.argv)
    else:
        wrapper = LinkerWrapper(sys.argv)
elif executable == 'ar':
    wrapper = ArWrapper(sys.argv)
elif executable == 'ranlib':
    # ranlib creates an index in the .a file -> we can skip this since we have created a llvm bitcode file
    wrapper = RanlibWrapper(sys.argv)
# direct invocation of the linker: can be ld, gold or lld
elif executable in ('ld', 'lld', 'gold'):
    wrapper = LinkerWrapper(sys.argv)
# we also need to wrap mv, ln and cp commands to make sure that our bitcode files end up in the right dir
elif executable == 'mv':
    wrapper = MvWrapper(sys.argv)
elif executable == 'ln':
    wrapper = LnWrapper(sys.argv)
elif executable == 'cp':
    wrapper = CpWrapper(sys.argv)
else:
    raise RuntimeError('Could not parse command line to determine what\'s happening: ', sys.argv)

wrapper.run()
