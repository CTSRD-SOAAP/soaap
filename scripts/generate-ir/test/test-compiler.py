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
