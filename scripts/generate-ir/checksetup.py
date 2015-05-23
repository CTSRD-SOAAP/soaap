#
# Copyright (C) 2015  Alex Richardson <alr48@cam.ac.uk>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
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
