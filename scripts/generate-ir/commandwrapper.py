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

import os
import sys
import subprocess
from enum import Enum
from termcolor import colored

#  TODO: let cmake set this at configure time
SOAAP_LLVM_BINDIR = os.getenv('SOAAP_LLVM_BINDIR', '/home/alex/devel/soaap/llvm/release-build/bin/')
if not os.path.isdir(SOAAP_LLVM_BINDIR):
    sys.exit('could not find SOAAP_LLVM_BINDIR, please make sure the env var is set correctly')


def errorMsg(msg):
    if not os.isatty(sys.stdout.fileno()):
        return msg
    return colored(msg, 'red', 'on_blue', attrs=['bold'])


def infoMsg(msg):
    if not os.isatty(sys.stdout.fileno()):
        return msg
    return colored(msg, 'magenta', 'on_green', attrs=[])


def warningMsg(msg):
    if not os.isatty(sys.stdout.fileno()):
        return msg
    return colored(msg, 'yellow', attrs=['bold'])


def highlightForMode(mode, msg):
    if not os.isatty(sys.stdout.fileno()):
        return msg
    if mode == Mode.object_file:
        return colored(msg, 'blue', attrs=['bold'])
    elif mode == Mode.static_lib:
        return colored(msg, 'magenta', attrs=['bold'])
    elif mode == Mode.shared_lib:
        return colored(msg, 'red', attrs=['bold'])
    elif mode == Mode.executable:
        return colored(msg, 'green', attrs=['bold'])
    elif mode == Mode.ranlib:
        return colored(msg, 'yellow', attrs=['bold'])
    else:
        print(warningMsg('WARNING: invalid mode: ' + mode.name))
        return infoMsg(msg)


def soaapLlvmBinary(name):
    wrapper = os.path.join(SOAAP_LLVM_BINDIR, name)
    if os.path.isfile(wrapper):
        return wrapper
    return None


# Find executable in $PATH
# http://stackoverflow.com/questions/377017/test-if-executable-exists-in-python/377028#377028
def findExe(program):
    def is_exe(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if is_exe(program):
            return program
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            exe_file = os.path.join(path, program)
            if is_exe(exe_file):
                return exe_file

    return None


def correspondingBitcodeName(fname):
    # if the output file is something like libfoo.so.1.2.3 we want libfoo.so.bc.1.2.3 to be emitted
    if '.so.' in fname:
        return fname.replace('.so.', '.so.bc.')
    return str(fname) + '.bc'


class Mode(Enum):
    unknown = 0  # this is an error
    shared_lib = 1
    static_lib = 2
    object_file = 3
    executable = 4
    ranlib = 5  # not really needed


class CommandWrapper:
    def __init__(self, originalCommandLine):
        # TODO: should we wrap gcc/g++/cc/c++?
        # if executable in ('clang', 'gcc', 'cc'):
        #    executable = 'clang'
        # elif executable in ('clang++', 'g++', 'c++'):
        #    executable = 'clang++'
        # remove spaces make a copy and remove spaces
        self.realCommand = [item.strip() for item in originalCommandLine]
        self.generateIrCommand = list()
        self.nothingToDo = False
        self.executable = str(self.realCommand[0])
        self.mode = Mode.unknown
        self.output = ''
        if os.getenv('LLVM_IR_WRAPPER_DELEGATE_TO_SYSTEM_COMPILER'):
            # clang or clang++ from $PATH
            self.realCommand[0] = findExe(self.executable)
        else:
            # SOAAP clang or clang++ or fallback to system binary
            self.realCommand[0] = soaapLlvmBinary(self.executable) or findExe(self.executable)
        assert self.realCommand[0]

    def run(self):
        if os.getenv('NO_EMIT_LLVM_IR'):
            os.execv(self.realCommand[0], self.realCommand)
            raise RuntimeError('execve failed!')

        # parse the original command line and compute the IR generation one
        self.computeWrapperCommand()

        if len(self.generateIrCommand) == 0 and not self.nothingToDo:
            raise RuntimeError('Could not determine IR command line from', self.realCommand)

        if self.nothingToDo:
            # no need to run the llvm generation command, only run the original instead
            print(highlightForMode(self.mode, self.executable + ' replacement: nothing to do:' + str(self.realCommand)))
        else:
            #  Do the IR generation first so that if it fails we don't have the Makefile dependencies existing!
            self.runGenerateIrCommand()
        # do the actual compilation step:
        self.runRealCommand()

    # allow overriding this for creating empty output files
    def runGenerateIrCommand(self):
        self.runCommand('LLVM IR:', self.generateIrCommand)

    def runRealCommand(self):
        self.runCommand('Original:', self.realCommand)

    def runCommand(self, msg, command):
        print(highlightForMode(self.mode, msg + ' ' + str(command)))
        subprocess.check_call(command)

    def createEmptyBitcodeFile(self, output):
        self.runCommand('LLVM IR:', [soaapLlvmBinary('clang'), '-c', '-emit-llvm', '-o', output,
                                     '-x', 'c', '/dev/null'])

    def computeWrapperCommand(self):
        raise NotImplementedError


# All clang parameters that expect an argument as next parameters:
# clang --help-hidden | grep -E -e '\-\w+ <'
#
# -ccc-install-dir <value>
# -ccc-arcmt-migrate <value>
# -ccc-gcc-name <gcc-path>
# -ccc-objcmt-migrate <value>
# -arcmt-migrate-report-output <value>
# -cxx-isystem <directory>
# -dependency-dot <value> Filename to write DOT-formatted header dependencies to
# -dependency-file <value>
# -fmodules-user-build-path <directory>
# -F <value>              Add directory to framework include search path
# -idirafter <value>      Add directory to AFTER include search path
# -iframework <value>     Add directory to SYSTEM framework search path
# -imacros <file>         Include macros from file before parsing
# -include-pch <file>     Include precompiled header file
# -include <file>         Include file before parsing
# -iprefix <dir>          Set the -iwithprefix/-iwithprefixbefore prefix
# -iquote <directory>     Add directory to QUOTE include search path
# -isysroot <dir>         Set the system root directory (usually /)
# -isystem <directory>    Add directory to SYSTEM include search path
# -ivfsoverlay <value>    Overlay the virtual filesystem described by file over the real file system
# -iwithprefixbefore <dir>
# -iwithprefix <dir>      Set directory to SYSTEM include search path with prefix
# -iwithsysroot <directory>
# -I <value>              Add directory to include search path
# -MF <file>              Write depfile output from -MMD, -MD, -MM, or -M to <file>
# -mllvm <value>          Additional arguments to forward to LLVM's option processing
# -module-dependency-dir <value>
# -MQ <value>             Specify name of main file output to quote in depfile
# -MT <value>             Specify name of main file output in depfile
# -o <file>               Write output to <file>
# -resource-dir <value>   The directory which holds the compiler resource files
# -serialize-diagnostics <value>
# -working-directory <value>
# -Xanalyzer <arg>        Pass <arg> to the static analyzer
# -Xassembler <arg>       Pass <arg> to the assembler
# -Xclang <arg>           Pass <arg> to the clang compiler
# -Xlinker <arg>          Pass <arg> to the linker
# -Xpreprocessor <arg>    Pass <arg> to the preprocessor
# -x <language>           Treat subsequent input files as having type <language>
# -z <arg>                Pass -z <arg> to the linker
#
# And gcc: gcc -v --help | grep -E -e '\-\w+ <'
#
# -Xassembler <arg>        Pass <arg> on to the assembler
# -Xpreprocessor <arg>     Pass <arg> on to the preprocessor
# -Xlinker <arg>           Pass <arg> on to the linker
# -B <directory>           Add <directory> to the compiler's search paths
# -o <file>                Place the output into <file>
# -x <language>            Specify the language of the following input files
# -F <dir>                    Add <dir> to the end of the main framework
# -I <dir>                    Add <dir> to the end of the main include path
# -MF <file>                  Write dependency output to the given file
# -MQ <target>                Add a MAKE-quoted target
# -MT <target>                Add an unquoted target
# -idirafter <dir>            Add <dir> to the end of the system include path
# -imacros <file>             Accept definition of macros in <file>
# -imultilib <dir>            Set <dir> to be the multilib include subdirectory
# -include <file>             Include the contents of <file> before other files
# -iprefix <path>             Specify <path> as a prefix for next two options
# -iquote <dir>               Add <dir> to the end of the quote include path
# -isysroot <dir>             Set <dir> to be the system root directory
# -isystem <dir>              Add <dir> to the start of the system include path
# -iwithprefix <dir>          Add <dir> to the end of the system include path
# -iwithprefixbefore <dir>    Add <dir> to the end of the main include path
# -o <file>                   Place output into <file>
# --param <param>=<value>     Set parameter <param> to value.  See below for a
# -aux-info <file>            Emit declaration information into <file>
# -dumpbase <file>            Set the file basename to be used for dumps
# -dumpdir <dir>              Set the directory name to be used for dumps
#
# -imultiarch <dir>           Set <dir> to be the multiarch include subdirectory
#
# --base_file <basefile>             Generate a base file for relocatable DLLs
# --file-alignment <size>            Set file alignment
# --heap <size>                      Set initial size of the heap
# --image-base <address>             Set start address of the executable
# --major-image-version <number>     Set version number of the executable
# --major-os-version <number>        Set minimum required OS version
# --major-subsystem-version <number> Set minimum required OS subsystem version
# --minor-image-version <number>     Set revision number of the executable
# --minor-os-version <number>        Set minimum required OS revision
# --minor-subsystem-version <number> Set minimum required OS subsystem revision
# --section-alignment <size>         Set section alignment
# --stack <size>                     Set size of the initial stack
# --subsystem <name>[:<version>]     Set required OS subsystem [& version]
# --out-implib <file>                Generate import library
# --output-def <file>                Generate a .DEF file for the built DLL
# --base_file <basefile>             Generate a base file for relocatable DLLs
# --file-alignment <size>            Set file alignment
# --heap <size>                      Set initial size of the heap
# --image-base <address>             Set start address of the executable
# --major-image-version <number>     Set version number of the executable
# --major-os-version <number>        Set minimum required OS version
# --major-subsystem-version <number> Set minimum required OS subsystem version
# --minor-image-version <number>     Set revision number of the executable
# --minor-os-version <number>        Set minimum required OS revision
# --minor-subsystem-version <number> Set minimum required OS subsystem revision
# --section-alignment <size>         Set section alignment
# --stack <size>                     Set size of the initial stack
# --subsystem <name>[:<version>]     Set required OS subsystem [& version]
# --out-implib <file>                Generate import library
# --output-def <file>                Generate a .DEF file for the built DLL

# TODO: check if I missed some
#
# -L <dir> as two params is deprecated but still supported
#
# TODO: does -lfoo work with a space as well?
# use set() to make sure that we don't have them twice
_CLANG_PARAMS_WITH_ARGUMENTS = None


def clangParamsWithArgument():
    global _CLANG_PARAMS_WITH_ARGUMENTS
    if _CLANG_PARAMS_WITH_ARGUMENTS:
        return _CLANG_PARAMS_WITH_ARGUMENTS
    # use a set to make sure they are only included once (might also make search faster)
    # generated by running --help and then replacing "^\s*(\-[\w|\-]+)(\s*<.*)" with "        '\1',  #\2"
    _CLANG_PARAMS_WITH_ARGUMENTS = frozenset([
        # -L <dir> is not included here, but it works
        '-L',  # <dir> Add <dir> to linker search path
        # clang --help-hidden | grep -E -e '\-(\w|\-)+ <' | sort
        '-arcmt-migrate-report-output',  # <value>
        '-ccc-arcmt-migrate',  # <value>
        '-ccc-gcc-name',  # <gcc-path>
        '-ccc-install-dir',  # <value>
        '-ccc-objcmt-migrate',  # <value>
        '-cxx-isystem',  # <directory>
        '-dependency-dot',  # <value> Filename to write DOT-formatted header dependencies to
        '-dependency-file',  # <value>
        '-fmodules-user-build-path',  # <directory>
        '-F',  # <value>              Add directory to framework include search path
        '-idirafter',  # <value>      Add directory to AFTER include search path
        '-iframework',  # <value>     Add directory to SYSTEM framework search path
        '-imacros',  # <file>         Include macros from file before parsing
        '-include',  # <file>         Include file before parsing
        '-include-pch',  # <file>     Include precompiled header file
        '-iprefix',  # <dir>          Set the -iwithprefix/-iwithprefixbefore prefix
        '-iquote',  # <directory>     Add directory to QUOTE include search path
        '-isysroot',  # <dir>         Set the system root directory (usually /)
        '-isystem',  # <directory>    Add directory to SYSTEM include search path
        '-I',  # <value>              Add directory to include search path
        '-ivfsoverlay',  # <value>    Overlay the virtual filesystem described by file over the real file system
        '-iwithprefixbefore',  # <dir>
        '-iwithprefix',  # <dir>      Set directory to SYSTEM include search path with prefix
        '-iwithsysroot',  # <directory>
        '-MF',  # <file>              Write depfile output from -MMD, -MD, -MM, or -M to <file>
        '-mllvm',  # <value>          Additional arguments to forward to LLVM's option processing
        '-module-dependency-dir',  # <value>
        '-MQ',  # <value>             Specify name of main file output to quote in depfile
        '-MT',  # <value>             Specify name of main file output in depfile
        '-o',  # <file>               Write output to <file>
        '-resource-dir',  # <value>   The directory which holds the compiler resource files
        '-serialize-diagnostics',  # <value>
        '-working-directory',  # <value>
        '-Xanalyzer',  # <arg>        Pass <arg> to the static analyzer
        '-Xassembler',  # <arg>       Pass <arg> to the assembler
        '-Xclang',  # <arg>           Pass <arg> to the clang compiler
        '-x',  # <language>           Treat subsequent input files as having type <language>
        '-Xlinker',  # <arg>          Pass <arg> to the linker
        '-Xpreprocessor',  # <arg>    Pass <arg> to the preprocessor
        '-z',  # <arg>  Pass -z <arg> to the linker
        #
        # Now GCC arguments (gcc -v --help | grep -E -e '\-(\w|\-)+ <'):
        # mostly the same, but let set deduplicate for us
        #
        '-aux-info',  # <file>            Emit declaration information into <file>
        '--base_file',  # <basefile>             Generate a base file for relocatable DLLs
        '-B',  # <directory>           Add <directory> to the compiler's search paths
        '-dumpbase',  # <file>            Set the file basename to be used for dumps
        '-dumpdir',  # <dir>              Set the directory name to be used for dumps
        '-F',  # <dir>                    Add <dir> to the end of the main framework
        '--file-alignment',  # <size>            Set file alignment
        '--heap',  # <size>                      Set initial size of the heap
        '-I',  # <dir>                    Add <dir> to the end of the main include path
        '-idirafter',  # <dir>            Add <dir> to the end of the system include path
        '-imacros',  # <file>             Accept definition of macros in <file>
        '--image-base',  # <address>             Set start address of the executable
        '-imultiarch',  # <dir>           Set <dir> to be the multiarch include subdirectory
        '-imultilib',  # <dir>            Set <dir> to be the multilib include subdirectory
        '-include',  # <file>             Include the contents of <file> before other files
        '-iprefix',  # <path>             Specify <path> as a prefix for next two options
        '-iquote',  # <dir>               Add <dir> to the end of the quote include path
        '-isysroot',  # <dir>             Set <dir> to be the system root directory
        '-isystem',  # <dir>              Add <dir> to the start of the system include path
        '-iwithprefixbefore',  # <dir>    Add <dir> to the end of the main include path
        '-iwithprefix',  # <dir>          Add <dir> to the end of the system include path
        '--major-image-version',  # <number>     Set version number of the executable
        '--major-os-version',  # <number>        Set minimum required OS version
        '--major-subsystem-version',  # <number> Set minimum required OS subsystem version
        '-MF',  # <file>                  Write dependency output to the given file
        '--minor-image-version',  # <number>     Set revision number of the executable
        '--minor-os-version',  # <number>        Set minimum required OS revision
        '--minor-subsystem-version',  # <number> Set minimum required OS subsystem revision
        '-MQ',  # <target>                Add a MAKE-quoted target
        '-MT',  # <target>                Add an unquoted target
        '-o',  # <file>                   Place output into <file>
        '--out-implib',  # <file>                Generate import library
        '--output-def',  # <file>                Generate a .DEF file for the built DLL
        '--param',  # <param>=<value>     Set parameter <param> to value.  See below for a
        '--section-alignment',  # <size>         Set section alignment
        '--stack',  # <size>                     Set size of the initial stack
        '--subsystem',  # <name>[:<version>]     Set required OS subsystem [& version]
        '-Xassembler',  # <arg>        Pass <arg> on to the assembler
        '-x',  # <language>            Specify the language of the following input files
        '-Xlinker',  # <arg>           Pass <arg> on to the linker
        '-Xpreprocessor',  # <arg>     Pass <arg> on to the preprocessor
    ])
    return _CLANG_PARAMS_WITH_ARGUMENTS
