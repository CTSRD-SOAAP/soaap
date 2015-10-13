#!/usr/bin/env python3

/*
 * Copyright (c) 2015 Alex Richardson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

import sys
import os

from linkerwrapper import *
from compilerwrapper import CompilerWrapper
from unixcommandswrapper import *

# TODO: rewrite this in C++, the python startup time of ~100-300ms per command really kills ./configure and make

# TODO: do we need to wrap objcopy?

executable = os.path.basename(sys.argv[0])
if executable in ('gcc', 'cc'):
    executable = 'clang'
if executable == ('g++', 'c++'):
    executable = 'clang++'
sys.argv[0] = executable

wrapper = None
if executable in ('clang', 'clang++'):
    # -c -> object file, -S -> generate ASM, -E -> run preprocessor only
    if any(x in sys.argv for x in ("-c", "-S", "-E")):
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
elif executable == 'install':
    wrapper = InstallWrapper(sys.argv)
else:
    raise RuntimeError('Could not parse command line to determine what\'s happening: ', sys.argv)

wrapper.run()
