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
sys.path.insert(0, os.path.abspath(".."))

import linkerwrapper


def getIrCommand(realCmd):
    wrapper = linkerwrapper.ArWrapper(realCmd)
    wrapper.computeWrapperCommand()
    result = list(wrapper.generateIrCommand)
    # we don't care about the full path to the compiler here
    result[0] = realCmd[0]
    return result


class TestArWrapper(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tempdir = tempfile.TemporaryDirectory()
        # print(cls.tempdir.name)
        os.chdir(cls.tempdir.name)
        # print(os.getcwd())
        cls.bitcodeFile = open(os.path.join(cls.tempdir.name, 'foo.o.bc'), 'x')

    @classmethod
    def tearDownClass(cls):
        os.chdir('/')
        cls.bitcodeFile.close()
        cls.tempdir.cleanup()

    def testBasic(self):
        command = getIrCommand(['ar', '-cqs', 'foo.o'])
        self.assertEqual(command, ['clang++', '-c', 'foo.c', '-o', 'foo.o.bc',
                                   '-gline-tables-only', '-emit-llvm', '-fno-inline'])
        command = getIrCommand(['clang++', '-o', '.obj/foo.o', '-c', 'foo.c'])
        self.assertEqual(command, ['clang++', '-o', '.obj/foo.o.bc', '-c', 'foo.c',
                                   '-gline-tables-only', '-emit-llvm', '-fno-inline'])


if __name__ == '__main__':
    unittest.main()
