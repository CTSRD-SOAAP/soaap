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
import tempfile

import unittest
import os
import sys
import tempfile
sys.path.insert(0, os.path.abspath(".."))

import linkerwrapper
from commandwrapper import Mode


def getIrCommand(realCmd):
    wrapper = linkerwrapper.LinkerWrapper(realCmd)
    wrapper.computeWrapperCommand()
    result = list(wrapper.generateIrCommand)
    # we don't want the soaap path
    result[0] = os.path.basename(result[0])
    return result


def removeLibs(command):
    return [item for item in command if not item.startswith('-l')]


class TestLinkerWrapper(unittest.TestCase):
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

    def testMode(self):
        wrapper = linkerwrapper.LinkerWrapper('clang -shared foo.o'.split())
        wrapper.parseCommandLine()
        self.assertEqual(wrapper.mode, Mode.shared_lib)
        wrapper = linkerwrapper.LinkerWrapper('clang++ -o libfoo.so.1.0.0 foo.o'.split())
        wrapper.parseCommandLine()
        self.assertEqual(wrapper.mode, Mode.shared_lib)
        wrapper = linkerwrapper.LinkerWrapper('clang foo.o'.split())
        wrapper.parseCommandLine()
        self.assertEqual(wrapper.mode, Mode.executable)
        wrapper = linkerwrapper.LinkerWrapper('clang foo.o -o foo'.split())
        wrapper.parseCommandLine()
        self.assertEqual(wrapper.mode, Mode.executable)

    def testBasic(self):
        # no output flag -> a.out.bc
        command = removeLibs(getIrCommand(['clang', 'foo.o']))
        self.assertEqual(command, ['llvm-link', '-o', 'a.out.bc', 'foo.o.bc'])
        # same if we are compiling a shared lib
        command = removeLibs(getIrCommand(['clang', '-shared', 'foo.o']))
        self.assertEqual(command, ['llvm-link', '-o', 'a.out.bc', 'foo.o.bc'])
        # output flag exists (including subdir)
        command = removeLibs(getIrCommand(['clang++', 'foo.o', '-o', '.libs/libfoo.so']))
        self.assertEqual(command, ['llvm-link', '-o', '.libs/libfoo.so.bc',  'foo.o.bc'])
        # test versioned .so file as output
        command = removeLibs(getIrCommand(['clang++', 'foo.o', '-o', '.libs/libfoo.so.1.2.3']))
        self.assertEqual(command, ['llvm-link', '-o', '.libs/libfoo.so.bc.1.2.3',  'foo.o.bc'])

    def testRemoveInvalidArgs(self):
        wrapper = linkerwrapper.LinkerWrapper(['clang', 'foo.o', '-fexcess-precision=standard'])
        wrapper.parseCommandLine()
        self.assertNotIn('-fexcess-precision=standard', wrapper.generateIrCommand)
        self.assertNotIn('-fexcess-precision=standard', wrapper.realCommand)

    def testAddStdlib(self):
        # clang adds libc
        command = getIrCommand(['clang', '-shared', 'foo.o', '-o', 'libfoo.so'])
        self.assertIn('-lc', command)
        self.assertNotIn('-lc++', command)
        # clang++ adds libc and libc++
        command = getIrCommand(['clang++', '-shared', 'foo.o', '-o', 'libfoo.so'])
        self.assertIn('-lc', command)
        self.assertIn('-lc++', command)
        # should not be added with -ffreestanding
        command = getIrCommand(['clang', '-shared', '-ffreestanding', 'foo.o', '-o', 'libfoo.so'])
        self.assertNotIn('-lc', command)
        self.assertNotIn('-lc++', command)
        command = getIrCommand(['clang++', '-shared', '-ffreestanding', 'foo.o', '-o', 'libfoo.so'])
        self.assertNotIn('-lc', command)
        self.assertNotIn('-lc++', command)


if __name__ == '__main__':
    unittest.main()
