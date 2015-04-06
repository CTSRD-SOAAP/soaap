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
#

import os
from commandwrapper import *


class CompilerWrapper(CommandWrapper):
    def __init__(self, originalCommandLine):
        # TODO: do we need to support -S flag (generate assembly)
        # And then of course we also need to wrap the assembler...
        super().__init__(originalCommandLine)
        self.mode = Mode.object_file

    def computeWrapperCommand(self):
        assert '-c' in self.realCommand

        self.mode = Mode.object_file
        self.generateIrCommand.append(soaapLlvmBinary(self.executable))  # clang or clang++

        skipNext = True  # skip executable
        inputFiles = []
        for index, param in enumerate(self.realCommand):
            if skipNext:
                skipNext = False
                continue
            elif param.startswith('-'):
                # TODO: strip optimization flags?
                if param.startswith('-g'):
                    # strip all -g parameters, we only want '-gline-tables-only'
                    continue
                elif param == '-finline':
                    continue
                elif param in clangParamsWithArgument():
                    if index + 1 >= len(self.realCommand):
                        raise RuntimeError(param + 'flag without parameter!', self.realCommand)
                    skipNext = True
                    next = self.realCommand[index + 1]
                    if param == '-o':
                        next = correspondingBitcodeName(next)
                        self.output = next
                    self.generateIrCommand.append(param)
                    self.generateIrCommand.append(next)
                else:
                    self.generateIrCommand.append(param)
            else:
                # this must be an input file
                inputFiles.append(param)
                self.generateIrCommand.append(param)

        if len(inputFiles) == 0:
            raise RuntimeError('No input files found!', self.realCommand)
        if not self.output:
            if len(inputFiles) != 1:
                raise RuntimeError('No -o flag, but multiple input files!', inputFiles, self.realCommand)
            root, ext = os.path.splitext(inputFiles[0])  # src/foo.c -> ('src/foo', '.c')
            self.output = correspondingBitcodeName(root + '.o')
            self.generateIrCommand.append('-o')
            self.generateIrCommand.append(self.output)

        # FIXME: Is -gline-tables-only enough?
        self.generateIrCommand.append('-gline-tables-only')  # soaap needs debug info
        self.generateIrCommand.append('-emit-llvm')
        self.generateIrCommand.append('-fno-inline')  # make sure functions are not inlined
