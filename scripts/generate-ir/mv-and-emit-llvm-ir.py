#!/usr/bin/env python3

import sys
import os

from commandwrapper import CommandWrapperError
from coreutilswrapper import *


# why is this not in the stdlib...
def strip_end(text, suffix):
    text = str(text)
    if not text.endswith(suffix):
        return text
    return text[:len(text) - len(suffix)]

executable = strip_end(os.path.basename(sys.argv[0]), "-and-emit-llvm-ir.py")
sys.argv[0] = executable

wrapper = None
if executable == 'mv':
    wrapper = MvWrapper(sys.argv)
elif executable == 'ln':
    wrapper = LnWrapper(sys.argv)
elif executable == 'cp':
    wrapper = CpWrapper(sys.argv)
else:
    raise CommandWrapperError('Could not parse command line to determine what\'s happening: ', sys.argv)

wrapper.run()
