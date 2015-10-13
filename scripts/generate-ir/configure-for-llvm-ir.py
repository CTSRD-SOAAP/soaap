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

import argparse
import os
import sys
import pprint
from checksetup import *

changedEnvVars = {}


def changeEnvVar(var, value):
    os.environ[var] = value
    changedEnvVars[var] = value
    print('set', var, 'to', value)


def setIrWrapperEnvVar(var, command):
    splitted = command.split()
    command = splitted[0]
    wrapper = os.path.join(IR_WRAPPER_DIR, 'bin', command)
    if not os.path.exists(wrapper):
        sys.exit('could not find ' + wrapper)
    # allow parameters to be passed
    if len(splitted) > 1:
        wrapper = wrapper + ' ' + ' '.join(splitted[1:])
    changeEnvVar(var, wrapper)

parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
# parser.add_argument('options', nargs='*', help='Arguments to pass to configure')
parser.add_argument('-f', required=False, default='./configure', help='configure script override')
parser.add_argument('--ld', required=False, default='ld', type=str,
                    help='LD wrapper script name (and parameters)')
parser.add_argument('--link', required=False, default='', type=str,
                    help='LINK wrapper script name (and parameters)')
parser.add_argument('--ar', required=False, default='ar', type=str,
                    help='AR wrapper script name (and parameters)')
parser.add_argument('--ranlib', required=False, default='ranlib', type=str,
                    help='RANLIB wrapper script name (and parameters)')
parser.add_argument('--env', required=False, action='append', metavar=('VAR=VALUE'),
                    help='override env var [VAR] with [VALUE]. Can be repeated multiple times')
parser.add_argument('--cpp-linker', action='store_true', help='Use C++ compiler for linking')
parser.add_argument('--confirm', action='store_true', help='Confirm before running configure')
parsedArgs, unknownArgs = parser.parse_known_args()
configureScript = parsedArgs.f
print('Configure script is:', configureScript)
print(parsedArgs)
commandline = [configureScript]
commandline.extend(unknownArgs)  # append all the user passed flags

# no need to add an override option for CC and CXX, this can be done via --env
setIrWrapperEnvVar('CC', 'clang')
setIrWrapperEnvVar('LTCC', 'clang')
setIrWrapperEnvVar('CXX', 'clang++')
setIrWrapperEnvVar('AR', parsedArgs.ar)
setIrWrapperEnvVar('RANLIB', parsedArgs.ranlib)
# TODO: this one might cause problems...
if parsedArgs.cpp_linker:
    if parsedArgs.ld == 'clang':
        parsedArgs.ld = 'clang++'
    if parsedArgs.link == 'clang':
        parsedArgs.link = 'clang++'

setIrWrapperEnvVar('LD', parsedArgs.ld)
if parsedArgs.link:
    setIrWrapperEnvVar('LINK', parsedArgs.link)

# now replace the manual overrides:
for s in (parsedArgs.env or []):
    (var, value) = s.split('=')
    changeEnvVar(var.strip(), value.strip())

print('About to run', commandline, 'with the following env vars:\n', pprint.pformat(changedEnvVars))

if parsedArgs.confirm:
    result = input('Continue? [y/n] (y)').lower()
    if len(result) > 0 and result[0] != 'y':
        sys.exit()

os.environ[ENVVAR_NO_EMIT_IR] = '1'  # very important, otherwise checks might fail!
# no need for subprocess.call, just use execve
os.execvpe(commandline[0], commandline, os.environ)
sys.exit('Could not execute ' + str(commandline))
