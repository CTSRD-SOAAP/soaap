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
import shlex
sys.path.insert(0, os.path.abspath(".."))

from unixcommandswrapper import *


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
