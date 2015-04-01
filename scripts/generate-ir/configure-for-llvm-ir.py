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


def setIrWrapperEnvVar(var, command):
    wrapper = os.path.join(scriptDir, command + '-and-emit-llvm-ir.py')
    if not os.path.exists(wrapper):
        sys.exit('could not find ' + wrapper)
    changeEnvVar(var, wrapper)

parser = argparse.ArgumentParser(formatter_class=argparse.ArgumentDefaultsHelpFormatter)
parser.add_argument('options', nargs='*', help='Arguments to pass to configure')
parser.add_argument('-f', required=False, default='./configure', help='configure script override')
parser.add_argument('--ld', required=False, default='clang', type=str,
                    help='LD variable override (wrapper script name)')
parser.add_argument('--link', required=False, default='clang', type=str,
                    help='LINK variable override (wrapper script name)')
parser.add_argument('--ar', required=False, default='ar', type=str,
                    help='AR variable override (wrapper script name)')
parser.add_argument('--ranlib', required=False, default='ar', type=str,
                    help='RANLIB variable override (wrapper script name)')
parser.add_argument('--var-override', nargs=2, required=False, action='append', metavar=('VAR', 'VALUE'),
                    help='override env var [var] with [value]. Can be repeated multiple times')
parser.add_argument('--confirm', action='store_true', help='Confirm before running configure')
parsedArgs, unknownArgs = parser.parse_known_args()
configureScript = parsedArgs.f
print('Configure script is:', configureScript)
print(parsedArgs)
commandline = [configureScript]
commandline.extend(unknownArgs)  # append all the user passed flags

setIrWrapperEnvVar('CC', 'clang')
setIrWrapperEnvVar('CXX', 'clang++')
setIrWrapperEnvVar('AR', parsedArgs.ar)
setIrWrapperEnvVar('RANLIB', parsedArgs.ranlib)
# TODO: this one might cause problems...
setIrWrapperEnvVar('LD', parsedArgs.ld)
setIrWrapperEnvVar('LINK', parsedArgs.link)

# now replace the manual overrides:
for i in (parsedArgs.var_override or []):
    changeEnvVar(i[0], i[1])

print('About to run', commandline, 'with the following env vars:\n', pprint.pformat(changedEnvVars))

if parsedArgs.confirm:
    result = input('Continue? [y/n] (y)').lower()
    if len(result) > 0 and result[0] != 'y':
        sys.exit()

os.environ['NO_EMIT_LLVM_IR'] = '1' #  very important, otherwise checks might fail!
# no need for subprocess.call, just use execve
os.execve(commandline[0], commandline, os.environ)
sys.exit('Could not execute ' + str(commandline))
