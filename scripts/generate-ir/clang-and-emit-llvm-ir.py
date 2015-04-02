#!/usr/bin/env python3

import sys
import subprocess
import os
import argparse
from termcolor import colored, cprint
from enum import Enum

# TODO: always build with optimizations off (does that include DCE?) or maybe -fno-inline is better?

# TODO: do we need to wrap objcopy?

# TODO: there is also an llvm-ar command, will those archives also work with llvm-link?
# or is it maybe better to use that instead of llvm-link?

# TODO: heuristics to skip LLVM IR generation in cmake or autoconf configure checks?

# TODO: handle -S flag (assembly generation): Do any build systems even use this?

# TODO: libtool messes up everything, work around that


class Mode(Enum):
    unknown = 0  # this is an error
    shared_lib = 1
    static_lib = 2
    object_file = 3
    executable = 4
    ranlib = 5  # not really needed


#  TODO: let cmake set this at configure time
SOAAP_LLVM_BINDIR = os.getenv('SOAAP_LLVM_BINDIR', '/home/alex/devel/soaap/llvm/release-build/bin/')
if not os.path.isdir(SOAAP_LLVM_BINDIR):
    sys.exit('could not find SOAAP_LLVM_BINDIR, please make sure the env var is set correctly')


def soaapLlvmBinary(name):
    wrapper = os.path.join(SOAAP_LLVM_BINDIR, name)
    if os.path.isfile(wrapper):
        return wrapper
    return None


# Work around clang++ possibly being aliased to this script -> find the real one
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


# why is this not in the stdlib...
def strip_end(text, suffix):
    text = str(text)
    if not text.endswith(suffix):
        return text
    return text[:len(text) - len(suffix)]


def errorMsg(msg):
    return colored(msg, 'red', 'on_blue', attrs=['bold'])


def infoMsg(msg):
    return colored(msg, 'magenta', 'on_green', attrs=[])


def warningMsg(msg):
    return colored(msg, 'yellow', attrs=['bold'])


def highlightForMode(mode, msg):
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


def findLinkInputCandidates(cmdline):
    skipNextParam = True   # skip the executable
    linkCandidates = []
    for param in cmdline:
        if skipNextParam:
            skipNextParam = False
            continue
        if param.startswith('-'):
            #
            # A lot more options from clang: clang --help-hidden | grep -E -e '\-\w+ <'
            # And gcc: gcc -v --help | grep -E -e '\-\w+ <'
            # G++ man page:
            # TODO: check if I missed some
            # -x <language> -l<library> -T <script> -Xlinker <option> -Xassembler <option> -u <symbol>
            # -idirafter <dir> -include <file>  -imacros <file> -iprefix <file>  -iwithprefix <dir>
            # -iwithprefixbefore <dir>  -isystem <dir> -imultilib <dir> -isysroot <dir>
            #
            # Qt uses the strange option -ccc-gcc-name g++
            #
            # -I dir -L dir as two params is deprecated but still supported
            #
            # TODO: how to handle link dirs (-L flag)
            # TODO: does -lfoo work with a space as well?
            if param in ('-I', '-o', '-L', '-u', '-Xlinker', '-T', '-x', '-include', '-isystem',
                         '-imultilib', '-isysroot', '-iprefix', '-iwithprefix', 'iwithprefixbefore',
                         '-idirafter', '-imacros', '-imultilib', '-Xassembler', '-ccc-gcc-name'):
                skipNextParam = True
            continue
            if param.startswith('-l') or param == '-pthread':
                # TODO: how to handle this? lookup global list of .bc libs? add it to metadata?
                print(warningMsg('ATTEMPTING TO LINK AGAINST: ' + param))
        if '.so' in param:
            print(warningMsg('Not adding ' + param + ' to the resulting binary to prevent duplicate symbols'))
            # TODO: how to properly handle shared libs, what about llvm-ar?
            # Add something to metadata?
        else:
            linkCandidates.append(param)

    return linkCandidates


#  check if a file with the name and .bc appended exists, if that fails
#  execute the `file` command to determine whether the file is a bitcode file
def findBitcodeFiles(files):
    found = []
    toTest = []
    for f in files:
        bitcodeName = f + '.bc'
        if os.path.exists(bitcodeName):
            print('found bitcode file', bitcodeName)
            found.append(bitcodeName)
        # work around libtool moving stuff around ....
        elif bitcodeName.startswith('.libs/'):
            bitcodeName = bitcodeName[len('.libs/'):]
            if os.path.exists(bitcodeName):
                print('found libtool bitcode file', f, '->', bitcodeName)
                found.append(bitcodeName)
            else:
                toTest.append(f)
        else:
            toTest.append(f)
    if len(toTest) > 0:
        # try:
        # Shouldn't be any errors here unless input is empty
        fileCmd = [findExe('file')]
        fileCmd.extend(toTest)
        print('running', fileCmd)
        fileTypes = subprocess.check_output(fileCmd).decode('utf-8')
        for index, line in enumerate(fileTypes.splitlines()):
            testedFile = toTest[index]
            if line.startswith(testedFile):
                line = line[len(testedFile) + 1:]  # remove the filename: start

            if 'LLVM IR bitcode' in line:
                found.append(testedFile)
            else:
                print(warningMsg('LLVM IR NOT FOUND for: ' + testedFile))
    # except:
    #    print(warningMsg('Unexpected error invoking file: '), sys.exc_info()[0])

    if len(found) == 0:
        print(warningMsg('No bitcode files found from input ' + str(files)))

    return found


# returns (outputStr, outputIndex) tuple
def getOutputParam(cmdline):
    outputIdx = -1
    output = None
    if '-o' in cmdline:
        outputIdx = cmdline.index('-o') + 1
        if outputIdx >= len(cmdline):
            sys.stderr.write('WARNING: -o flag given but no parameter to it!')
        else:
            output = cmdline[outputIdx]
    return (output, outputIdx)


# MAIN script:

# TODO: should we really wrap gcc/g++/cc/c++?
executable = strip_end(os.path.basename(sys.argv[0]), "-and-emit-llvm-ir.py")
if executable in ('clang', 'gcc', 'cc'):
    executable = 'clang'
elif executable in ('clang++', 'g++', 'c++'):
    executable = 'clang++'

# make sure we print everything to stdout so that we don't confuse code that parses stdout
origStdout = sys.stdout
sys.stdout = sys.stderr

# perform the actual compile:
# just use the normal clang++ executable
compile_cmdline = list(sys.argv)  # copy
map(str.strip, compile_cmdline)  # remove spaces
if os.getenv('LLVM_IR_WRAPPER_DELEGATE_TO_SYSTEM_COMPILER'):
    # system clang or clang++
    compile_cmdline[0] = findExe(executable)
else:
    # SOAAP clang or clang++ or fallback to system binary
    compile_cmdline[0] = soaapLlvmBinary(executable) or findExe(executable)
compile_mode = Mode.unknown
output = ''
generateIrCmdline = []
nothingToDo = False


# we don't want to interfere with the ./configure / CMake tests since they parse output
# -> set the environment variable NO_EMIT_LLVM_IR to skip it, etc.
if os.getenv('NO_EMIT_LLVM_IR'):
    os.execv(compile_cmdline[0], compile_cmdline)
    sys.exit('execve failed!')
# ./configure checks uses '-print-prog-name=ld' and  '-print-search-dirs'
# just forward to the real compiler and exit -> execve
if len(compile_cmdline) > 1 and compile_cmdline[1].startswith('-print'):
    os.execv(compile_cmdline[0], compile_cmdline)
    sys.exit('execve failed!')

# ar is used to add together .o files to a static library
# it can create such a lib but also add add members late
# TODO: how does this interact with the llvm-link module name option
if executable == 'ar':
    compile_mode = Mode.static_lib
    if len(compile_cmdline) < 4:
        sys.exit(errorMsg('ERROR: cannot parse AR invocation: ' + str(compile_cmdline)))
    operation = compile_cmdline[1]
    # for now we only understand append 'q' combined with 'c' (create)
    if 'r' in operation:
        # replacement, like q, but replaces existing members, hopefully this will work!
        # TODO: use llvm-ar instead? how will that work?
        pass
    elif not ('q' in operation and 'c' in operation):
        sys.exit(errorMsg('ERROR: only ar with \'cq\' mode is currently supported: ' + str(compile_cmdline)))

    output = compile_cmdline[2]
    generateIrCmdline = [soaapLlvmBinary('llvm-link'), '-o', output + '.bc']
    generateIrCmdline.extend(findBitcodeFiles(compile_cmdline[3:]))

# ranlib creates an index in the .a file -> we can skip this since we have created a llvm bitcode file
elif executable == 'ranlib':
    compile_mode = Mode.ranlib
    output = compile_cmdline[-1]
    outputIdx = len(compile_cmdline) - 1
    nothingToDo = True

# direct invocation of the linker: can be ld, gold or lld
elif executable in ('ld', 'lld', 'gold'):
    output, outputIdx = getOutputParam(compile_cmdline)
    compile_mode = Mode.shared_lib
    sys.exit(errorMsg('TODO: direct linker invocation: ' + str(compile_cmdline)))
elif executable in ('clang', 'clang++'):
    output, outputIdx = getOutputParam(compile_cmdline)
    if "-c" in compile_cmdline:
        # easy case: add .bc to output, change compiler to the SOAAP clang(++) and add '-emit-llvm -g'
        compile_mode = Mode.object_file
        generateIrCmdline = list(compile_cmdline)
        generateIrCmdline[0] = soaapLlvmBinary(executable)  # clang or clang++
        generateIrCmdline[0] = soaapLlvmBinary(executable)  # clang or clang++
        generateIrCmdline.append('-g')  # soaap needs debug info
        generateIrCmdline.append('-emit-llvm')
        generateIrCmdline.append('-fno-inline')  # make sure functions are not inlined
        # TODO: generateIrCmdline.append('-O0')  # should this be added?
        if outputIdx < 0:
            # passing -c and no -o flag will produce an output with the extension renamed to .o
            # in this case we can't replace but need to add to the command line
            cIndex = compile_cmdline.index('-c')
            if cIndex + 1 >= len(compile_cmdline):
                sys.exit(errorMsg('ERROR: could not determine output file (no -o and invalid -c): ' +
                                  str(compile_cmdline)))
            output = os.path.splitext(compile_cmdline[cIndex + 1])[0] + '.o.bc'
            generateIrCmdline.append('-o')
            generateIrCmdline.append(output)
        else:
            generateIrCmdline[outputIdx] = output + '.bc'
    else:
        # now we really need the output file name
        if outputIdx < 0:
            print(warningMsg('WARNING: could not determine output file: ' + str(compile_cmdline)))
            output = 'a.out'
        # must be linking if we aren't using -c
        if '-shared' in compile_cmdline or '.so' in output:
            # using clang and passing '-shared' passed to the compiler will create a shared lib
            compile_mode = Mode.shared_lib
        else:
            # This one is problematic, could be that it is directly invoking clang to compile sources to executables,
            # have to somehow handle that case -> maybe if commandline contains files ending in .c, .cpp, .cxx .c++
            # or are there any flags that we could detect? Or run file magic again
            compile_mode = Mode.executable

        generateIrCmdline = [soaapLlvmBinary('llvm-link'), '-o', output + '.bc']
        # we have already verified that the -o flag exists so there is no need to replace it
        # -> findLinkInputCandidates can skip it
        linkCandidates = findLinkInputCandidates(compile_cmdline)
        if len(linkCandidates) == 0:
            sys.exit(errorMsg('NO LINK CANDIDATES FOUND IN CMDLINE: ' + str(compile_cmdline)))
            nothingToDo = True
        inputFiles = findBitcodeFiles(linkCandidates)
        if len(inputFiles) == 0:
            print(warningMsg("NO FILES FOUND FOR LINKING!"))
            nothingToDo = True
        # print(infoMsg("InputFiles:" + str(inputFiles)))
        generateIrCmdline.extend(inputFiles)
        # print(len(generateIrCmdline), generateIrCmdline)
else:
    compile_mode = Mode.unknown
    sys.exit(errorMsg('Could not parse command line to determine what\'s happening: ' + str(compile_cmdline)))


print(highlightForMode(compile_mode, compile_mode.name + ' OUTPUT=' + output + '\nCMDLINE=' + str(generateIrCmdline) +
                       '\nORIG_CMDLINE=' + str(compile_cmdline)))

if '--simulate' not in sys.argv:
    #  Do the IR generation first so that if it fails we don't have the makefile dependencies existing!
    if nothingToDo:
        # no need to run the llvm generation command, only do the original instead
        print(infoMsg(executable + ' replacement: nothing to do:' + str(compile_cmdline)))
    elif len(generateIrCmdline) == 0:
        sys.exit(errorMsg('ERROR: could not determine IR command line!' + str(compile_cmdline)))
    else:
        # do the IR generation:
        subprocess.check_call(generateIrCmdline)
    # do the actual compilation step:
    sys.stdout = origStdout
    subprocess.check_call(compile_cmdline)
