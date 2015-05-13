#!/usr/bin/env python3
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

import unittest
import os
import sys
import tempfile
import shlex
sys.path.insert(0, os.path.abspath(".."))

from coreutilswrapper import *


def getIrCommand(realCmd):
    if realCmd[0] == "mv":
        wrapper = MvWrapper(realCmd)
    elif realCmd[0] == "ln":
        wrapper = LnWrapper(realCmd)
    else:
        raise RuntimeError()
    wrapper.computeWrapperCommand()
    # we only want the executable name not the full path
    if wrapper.generateIrCommand:
        wrapper.generateIrCommand[0] = os.path.basename(wrapper.generateIrCommand[0])
    result = list(wrapper.generateIrCommand)
    return result


class TestMvWrapper(unittest.TestCase):
    def testBasic(self):
        command = getIrCommand(shlex.split("mv -f foo.o bar.o"))
        self.assertListEqual(command, [])
        command = getIrCommand(shlex.split("mv -f foo.so bar.so"))
        self.assertListEqual(command, shlex.split("mv -f foo.so.bc bar.so.bc"))
        command = getIrCommand(shlex.split("mv -f foo.a bar.a"))
        self.assertListEqual(command, shlex.split("mv -f foo.a.bc bar.a.bc"))
        command = getIrCommand(shlex.split("mv -f foo.so.1.2.3 foo.so"))
        self.assertListEqual(command, shlex.split("mv -f foo.so.bc.1.2.3 foo.so.bc"))
        command = getIrCommand(shlex.split("mv -f foo.a.1.2.3 foo.a"))
        self.assertListEqual(command, shlex.split("mv -f foo.a.bc.1.2.3 foo.a.bc"))

    def testMoveMultipleToDir(self):
        command = getIrCommand(shlex.split("mv -f foo.a.1.2.3 foo.a lib"))
        self.assertListEqual(command, shlex.split("mv -f foo.a.bc.1.2.3 foo.a.bc lib"))

class TestLnWrapper(unittest.TestCase):
    def testBasic(self):
        command = getIrCommand(shlex.split("ln -s -f foo.o bar.o"))
        self.assertListEqual(command, [])
        command = getIrCommand(shlex.split("ln -s -f foo.so bar.so"))
        self.assertListEqual(command, shlex.split("ln -s -f foo.so.bc bar.so.bc"))
        command = getIrCommand(shlex.split("ln -sf foo.a bar.a"))
        self.assertListEqual(command, shlex.split("ln -sf foo.a.bc bar.a.bc"))
        command = getIrCommand(shlex.split("ln -sf foo.so.1.2.3 foo.so"))
        self.assertListEqual(command, shlex.split("ln -sf foo.so.bc.1.2.3 foo.so.bc"))
        command = getIrCommand(shlex.split("ln -sf foo.a.1.2.3 foo.a"))
        self.assertListEqual(command, shlex.split("ln -sf foo.a.bc.1.2.3 foo.a.bc"))

    def testMoveMultipleToDir(self):
        command = getIrCommand(shlex.split("ln -sf foo.a.1.2.3 foo.a lib"))
        self.assertListEqual(command, shlex.split("ln -sf foo.a.bc.1.2.3 foo.a.bc lib"))


if __name__ == '__main__':
    unittest.main()
