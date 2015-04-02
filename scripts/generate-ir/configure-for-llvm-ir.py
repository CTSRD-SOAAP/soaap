#!/usr/bin/env python3

import argparse
import os
import sys
import pprint

scriptDir = os.path.dirname(os.path.realpath(__file__))
changedEnvVars = {}


def changeEnvVar(var, value):
    os.environ[var] = value
    changedEnvVars[var] = value
    print('set', var, 'to', value)

def setIrWrapperEnvVar(var, command):
    splitted = command.split()
    command = splitted[0]
    wrapper = os.path.join(scriptDir, command + '-and-emit-llvm-ir.py')
    if not os.path.exists(wrapper):
        sys.exit('could not find ' + wrapper)
    # allow parameters to be passed
    if len(splitted) > 1:
        wrapper = wrapper + ' ' + ' '.join(splitted[1:])
    changeEnvVar(var, wrapper)

parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
# parser.add_argument('options', nargs='*', help='Arguments to pass to configure')
parser.add_argument('-f', required=False, default='./configure', help='configure script override')
parser.add_argument('--ld', required=False, default='clang', type=str,
                    help='LD wrapper script name (and parameters)')
parser.add_argument('--link', required=False, default='clang', type=str,
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

os.environ['NO_EMIT_LLVM_IR'] = '1' # very important, otherwise checks might fail!
# no need for subprocess.call, just use execve
os.execvpe(commandline[0], commandline, os.environ)
sys.exit('Could not execute ' + str(commandline))
