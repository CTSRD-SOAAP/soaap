#!/usr/bin/env python3

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
    if not wrapper.nothingToDo:
        assert "-libmd" in wrapper.generateIrCommand
        wrapper.generateIrCommand.remove("-libmd")
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
        self.assertEqual(command, "llvm-link foo.o.bc -o a.out.bc".split())
        # same if we are compiling a shared lib
        command = removeLibs(getIrCommand(['clang', '-shared', 'foo.o']))
        self.assertEqual(command, "llvm-link foo.o.bc -o a.out.bc".split())
        # output flag exists (including subdir)
        command = removeLibs(getIrCommand(['clang++', 'foo.o', '-o', '.libs/libfoo.so']))
        self.assertEqual(command, ['llvm-link', 'foo.o.bc', '-o', '.libs/libfoo.so.bc'])
        # test versioned .so file as output
        command = removeLibs(getIrCommand(['clang++', 'foo.o', '-o', '.libs/libfoo.so.1.2.3']))
        self.assertEqual(command, ['llvm-link', 'foo.o.bc', '-o', '.libs/libfoo.so.bc.1.2.3'])

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
        self.assertEqual(command, "llvm-link foo.o.bc -lbar -lc -o foo.bc".split())
        # absolute path with spaces
        command = getIrCommand(["clang", "-o", "foo", "foo.o", "/dir/with space/libbar.so"])
        self.assertEqual(command, "llvm-link foo.o.bc -lbar -lc -o foo.bc".split())
        # versioned absolute path (here we keep the suffix)
        command = getIrCommand("clang -o foo foo.o /usr/lib/libbar.so.1.2.3".split())
        self.assertEqual(command, "llvm-link foo.o.bc -lbar.so.bc.1.2.3 -lc -o foo.bc".split())
        # relative path
        command = getIrCommand("clang -o foo foo.o libs/libbar.so".split())
        self.assertEqual(command, "llvm-link foo.o.bc -lbar -lc -o foo.bc".split())
        # file name
        command = getIrCommand("clang -o foo foo.o libbar.so".split())
        self.assertEqual(command, "llvm-link foo.o.bc -lbar -lc -o foo.bc".split())
        # multiple libraries
        command = getIrCommand("clang -o foo foo.o libbar.so /lib/libm.so".split())
        self.assertEqual(command, "llvm-link foo.o.bc -lbar -lm -lc -o foo.bc".split())
        # not prefixed with lib -> for now this results in an error
        # TODO: if any project needs this figure out how to skip the 'lib' being automatically added in llvm-link
        with self.assertRaises(RuntimeError):
            getIrCommand("clang -o foo foo.o myplugin.so".split())

if __name__ == '__main__':
    unittest.main()
