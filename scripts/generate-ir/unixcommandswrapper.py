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
from commandwrapper import *


class CoreUtilsWrapper(CommandWrapper):
    def __init__(self, originalCommandLine):
        super().__init__(originalCommandLine)
        self.mode = Mode.coreutils
        self.needForce = False

    def computeWrapperCommand(self):
        haveLibs = False
        hasForceFlag = False
        self.generateIrCommand.append(self.realCommand[0])
        target = None
        for i in self.realCommand[1:]:
            if i.startswith("-"):
                if "f" in i:
                    hasForceFlag = True
                self.generateIrCommand.append(i)
                continue
            elif isLibrary(i) or i.endswith('.o'):
                # we have a .so or .a or .o that is being moved -> move the bitcode lib as well
                i = correspondingBitcodeName(i)
                haveLibs = True
            elif haveLibs:
                if i.endswith('.lo'):
                    # This happens e.g. with zlib build system:
                    # `mv objs/crc32.o crc32.lo`
                    i = correspondingBitcodeName(i)
                elif not os.path.isdir(i):
                    # we already have libs and this parameter is neither a directory (like when moving multiple files)
                    # nor a file which is typically a bitcode lib -> error out for now
                    raise CommandWrapperError("Could not determine correct wrapper command", sys.argv)
            self.generateIrCommand.append(i)

        if not hasForceFlag and self.needForce:
            self.generateIrCommand.insert(1, "-f")

        if not haveLibs:
            self.nothingToDo = True
            self.generateIrCommand = []


class MvWrapper(CoreUtilsWrapper):
    def __init__(self, originalCommandLine):
        super().__init__(originalCommandLine)
        self.needForce = True


class LnWrapper(CoreUtilsWrapper):
    def __init__(self, originalCommandLine):
        super().__init__(originalCommandLine)
        self.needForce = True


class CpWrapper(CoreUtilsWrapper):
    def __init__(self, originalCommandLine):
        super().__init__(originalCommandLine)
        # TODO: do we need -f here as well?


class InstallWrapper(CoreUtilsWrapper):
    def __init__(self, originalCommandLine):
        super().__init__(originalCommandLine)

    def computeWrapperCommand(self):
        # only do the installing of bitcode when explicitly requested
        if os.getenv(ENVVAR_INSTALL_BITCODE):
            super().computeWrapperCommand()
        else:
            self.nothingToDo = True
            self.generateIrCommand = []
