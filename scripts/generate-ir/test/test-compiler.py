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
import tempfile
import sys
sys.path.insert(0, os.path.abspath(".."))

from compilerwrapper import CompilerWrapper


def getWrapper(realCmd):
    if type(realCmd) == str:
        realCmd = realCmd.split()
    wrapper = CompilerWrapper(realCmd)
    wrapper.computeWrapperCommand()
    # we only want the executable name not the full path
    wrapper.generateIrCommand[0] = os.path.basename(wrapper.generateIrCommand[0])
    return wrapper


def getIrCommand(realCmd):
    return getWrapper(realCmd).generateIrCommand


class TestCompilerWrapper(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.tempdir = tempfile.TemporaryDirectory()
        # print(cls.tempdir.name)
        os.chdir(cls.tempdir.name)
        os.makedirs(os.path.join(cls.tempdir.name, 'src/fenv'))
        # print(os.getcwd())
        cls.cstubfile = open(os.path.join(cls.tempdir.name, 'src/fenv/fenv.c'), 'x')

    @classmethod
    def tearDownClass(cls):
        os.chdir('/')
        cls.cstubfile.close()
        cls.tempdir.cleanup()

    def testBasic(self):
        command = getIrCommand(['clang++', '-c', 'foo.c'])
        self.assertEqual(command, ['clang++', '-c', 'foo.c', '-gline-tables-only', '-emit-llvm', '-fno-inline',
                                   '-o', 'foo.o.bc'])
        command = getIrCommand(['clang++', '-o', '.obj/foo.o', '-c', 'foo.c'])
        self.assertEqual(command, ['clang++', '-c', 'foo.c', '-gline-tables-only', '-emit-llvm', '-fno-inline',
                                   '-o', '.obj/foo.o.bc'])

    def testSpacesRemoved(self):
        wrapper = getWrapper([' clang++', ' -Wall ',  '-c ', ' foo.c '])
        wrapper.computeWrapperCommand()
        for val in wrapper.realCommand:
            self.assertEqual(val, val.strip())
        for val in wrapper.generateIrCommand:
            self.assertEqual(val, val.strip())

    def testDebugFlag(self):
        self.assertIn('-gline-tables-only', getIrCommand(['clang++', '-c', 'foo.c']))
        command = getIrCommand(['clang++', '-g', '-c', 'foo.c'])
        self.assertIn('-gline-tables-only', command)
        self.assertNotIn('-g', command)  # -g must be stripped
        command = getIrCommand(['clang++', '-ggdb', '-c', 'foo.c'])
        self.assertIn('-gline-tables-only', command)
        self.assertNotIn('-g', command)  # same for -ggdb must be stripped

    def testInlineFlags(self):
        command = getIrCommand(['clang++', '-finline', '-c', 'foo.c'])
        self.assertIn('-fno-inline', command)
        self.assertNotIn('-finline', command)  # -finline mustn't be part of the command line

    def testMuslLibcASM(self):
        # work around musl libc asm files by compiling the stubs instead
        command = getIrCommand("clang++ -c src/fenv/x86_64/fenv.s")
        self.assertEqual(command, ['clang++', '-c', 'src/fenv/fenv.c', '-gline-tables-only', '-emit-llvm',
                                   '-fno-inline', '-o', 'src/fenv/fenv.o.bc'])

    def testRemoveInvalidArgs(self):
        wrapper = getWrapper(['clang++', '-fexcess-precision=standard', '-c', 'foo.c'])
        # -fexcess-precision=standard mustn't be part of the command line (real and generate IR)
        self.assertNotIn('-fexcess-precision=standard', wrapper.realCommand)
        self.assertNotIn('-fexcess-precision=standard', wrapper.generateIrCommand)

        # same again with the flag multiple times
        wrapper = getWrapper(['clang++', '-fexcess-precision=standard', '-c', 'foo.c', '-fexcess-precision=standard'])
        # -fexcess-precision=standard mustn't be part of the command line (real and generate IR)
        self.assertNotIn('-fexcess-precision=standard', wrapper.realCommand)
        self.assertNotIn('-fexcess-precision=standard', wrapper.generateIrCommand)

        # same again with '-frounding-math' as well
        wrapper = getWrapper("clang++ -fexcess-precision=standard -c -frounding-math foo.c")
        # -fexcess-precision=standard mustn't be part of the command line (real and generate IR)
        self.assertNotIn('-fexcess-precision=standard', wrapper.realCommand)
        self.assertNotIn('-fexcess-precision=standard', wrapper.generateIrCommand)
        self.assertNotIn('-frounding-math', wrapper.realCommand)
        self.assertNotIn('-frounding-math', wrapper.generateIrCommand)


if __name__ == '__main__':
    unittest.main()
