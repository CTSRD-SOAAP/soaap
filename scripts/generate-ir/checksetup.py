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

import os
import sys
import shlex
# make sure we have enum and termcolor
from enum import Enum

IR_WRAPPER_DIR = os.path.dirname(os.path.realpath(__file__))
ENVVAR_INSTALL_BITCODE = "LLVM_IR_WRAPPER_INSTALL_BITCODE"
ENVVAR_NO_EMIT_IR = "LLVM_IR_WRAPPER_NO_EMIT_LLVM_IR"
ENVVAR_DELEGATE_TO_SYSTEM_COMPILER = "LLVM_IR_WRAPPER_DELEGATE_TO_SYSTEM_COMPILER"
ENVVAR_SKIP_ASM_FILES = "LLVM_IR_WRAPPER_SKIP_ASSEMBLY_FILES"
ENVVAR_SKIP_MISSING_LINKER_INPUT = "LLVM_IR_WRAPPER_SKIP_MISSING_LINKER_INPUT"
ENVVAR_SOAAP_LLVM_BINDIR = "SOAAP_LLVM_BINDIR"

#  TODO: let cmake set this at configure time
SOAAP_LLVM_BINDIR = os.getenv(ENVVAR_SOAAP_LLVM_BINDIR, os.path.expanduser('~') + '/devel/soaap/llvm/build/bin/')
if not os.path.isdir(SOAAP_LLVM_BINDIR):
    sys.exit('could not find $' + ENVVAR_SOAAP_LLVM_BINDIR + ', please make sure the env var is set correctly')


def quoteCommand(command: list):
    newList = [shlex.quote(s) for s in command]
    return " ".join(newList)
