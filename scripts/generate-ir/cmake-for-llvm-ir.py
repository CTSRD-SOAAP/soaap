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
from checksetup import *
from commandwrapper import findExe


def irWrapper(var, command):
    wrapper = os.path.join(IR_WRAPPER_DIR, 'bin', command)
    if not os.path.exists(wrapper):
        sys.exit('could not find ' + wrapper)
    return '-DCMAKE_' + var + '=' + wrapper


commandline = [
    findExe('cmake'),
    irWrapper('C_COMPILER', 'clang'),
    irWrapper('CXX_COMPILER', 'clang++'),
    irWrapper('LINKER', 'ld'),
    irWrapper('AR', 'ar'),
    irWrapper('RANLIB', 'ranlib')
]
hasGenerator = any(elem.startswith('-G') for elem in sys.argv)
if not hasGenerator:
    commandline.append('-GNinja')
commandline.extend(sys.argv[1:])  # append all the user passed flags

print("About to run:", quoteCommand(commandline))
os.environ[ENVVAR_NO_EMIT_IR] = '1'
# no need for subprocess.call, just use execvpe
os.execvpe(commandline[0], commandline, os.environ)
sys.exit('Could not execute ' + str(commandline))
