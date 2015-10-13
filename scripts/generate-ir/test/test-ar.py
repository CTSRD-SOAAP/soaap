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
import sys
import tempfile
sys.path.insert(0, os.path.abspath(".."))

import linkerwrapper


def getIrCommand(realCmd):
    wrapper = linkerwrapper.ArWrapper(realCmd)
    wrapper.computeWrapperCommand()
    # we only want the executable name not the full path
    wrapper.generateIrCommand[0] = os.path.basename(wrapper.generateIrCommand[0])
    result = list(wrapper.generateIrCommand)
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

# TODO: handle multiple definitions
    def testBasic(self):
        command = getIrCommand("ar cqs foo foo.o".split())
        self.assertEqual(command, "llvm-link -libmd foo.o.bc -o foo.bc".split())
        command = getIrCommand("ar r foo foo.o".split())
        self.assertEqual(command, "llvm-link -libmd foo.o.bc -o foo.bc".split())

    def testMultipleDef(self):
        command = getIrCommand("ar r foo foo.o foo.o".split())
        self.assertEqual(command, "llvm-link -libmd foo.o.bc foo.o.bc -o foo.bc".split())

if __name__ == '__main__':
    unittest.main()
