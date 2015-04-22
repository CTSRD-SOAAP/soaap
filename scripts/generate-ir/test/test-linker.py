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

from linkerwrapper import LinkerWrapper
from commandwrapper import Mode


def getWrapper(realCmd):
    wrapper = LinkerWrapper(realCmd)
    wrapper.computeWrapperCommand()
    # we only want the executable name not the full path
    wrapper.generateIrCommand[0] = os.path.basename(wrapper.generateIrCommand[0])
    return wrapper


def getIrCommand(realCmd):
    return getWrapper(realCmd).generateIrCommand


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
        wrapper = getWrapper("clang -shared foo.o".split())
        self.assertEqual(wrapper.mode, Mode.shared_lib)
        wrapper = getWrapper("clang++ -o libfoo.so.1.0.0 foo.o".split())
        self.assertEqual(wrapper.mode, Mode.shared_lib)
        wrapper = getWrapper("clang foo.o".split())
        self.assertEqual(wrapper.mode, Mode.executable)
        wrapper = getWrapper("clang foo.o -o foo".split())
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
        wrapper = getWrapper(['clang', 'foo.o', '-fexcess-precision=standard'])
        # has to also be remove from the real command not just the IR generating one
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

    def testSharedLibsToLFlag(self):
        # absolute path
        command = getIrCommand("clang -o foo foo.o /usr/lib/libbar.so".split())
        self.assertEqual(command, "llvm-link -o foo.bc foo.o.bc -lbar -lc".split())
        # absolute path with spaces
        command = getIrCommand(["clang", "-o", "foo", "foo.o", "/dir/with space/libbar.so"])
        self.assertEqual(command, "llvm-link -o foo.bc foo.o.bc -lbar -lc".split())
        # versioned absolute path (here we keep the suffix)
        command = getIrCommand("clang -o foo foo.o /usr/lib/libbar.so.1.2.3".split())
        self.assertEqual(command, "llvm-link -o foo.bc foo.o.bc -lbar.so.bc.1.2.3 -lc".split())
        # relative path
        command = getIrCommand("clang -o foo foo.o libs/libbar.so".split())
        self.assertEqual(command, "llvm-link -o foo.bc foo.o.bc -lbar -lc".split())
        # file name
        command = getIrCommand("clang -o foo foo.o libbar.so".split())
        self.assertEqual(command, "llvm-link -o foo.bc foo.o.bc -lbar -lc".split())
        # multiple libraries
        command = getIrCommand("clang -o foo foo.o libbar.so /lib/libm.so".split())
        self.assertEqual(command, "llvm-link -o foo.bc foo.o.bc -lbar -lm -lc".split())
        # not prefixed with lib -> for now this results in an error
        # TODO: if any project needs this figure out how to skip the 'lib' being automatically added in llvm-link
        with self.assertRaises(RuntimeError):
            getIrCommand("clang -o foo foo.o myplugin.so".split())

if __name__ == '__main__':
    unittest.main()
