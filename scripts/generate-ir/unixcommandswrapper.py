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


class CoreUtilsWrapper(CommandWrapper):
    def __init__(self, originalCommandLine):
        super().__init__(originalCommandLine)
        self.mode = Mode.coreutils
        self.needForce = False

    def computeWrapperCommand(self):
        haveLibs = False
        hasForceFlag = False
        self.generateIrCommand.append(self.realCommand[0])
        for i in self.realCommand[1:]:
            if isLibrary(i):
                # we have a .so or .a that is being moved -> move the bitcode lib as well
                self.generateIrCommand.append(correspondingBitcodeName(i))
                haveLibs = True
                continue
            if i.startswith("-") and "f" in i:
                hasForceFlag = True
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
