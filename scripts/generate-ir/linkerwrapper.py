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

from commandwrapper import *
import tempfile
import shutil


class LinkerWrapper(CommandWrapper):

    def __init__(self, originalCommandLine):
        super().__init__(originalCommandLine)

    def computeWrapperCommand(self):
        if self.executable in ('ld', 'gold', 'lld') or self.executable.startswith('ld.'):
            raise NotImplementedError('Wrapping direct ld calls not supported yet!')
        else:
            assert self.executable.startswith('clang')

        self.parseCommandLine()
        if self.nothingToDo:
            return  # no need to look for the bitcode files if lib was explicitly skipped

        if len(self.linkCandidates) == 0:
            raise CommandWrapperError('NO LINK CANDIDATES FOUND IN CMDLINE: ', self.realCommand)

        self.generateIrCommand = [soaapLlvmBinary('llvm-link'), '-libmd']

        inputFiles = findBitcodeFiles(self.linkCandidates)
        if len(inputFiles) == 0:
            raise CommandWrapperError('NO FILES FOUND FOR LINKING!', self.realCommand)
        # print(infoMsg("InputFiles:" + str(inputFiles)))
        self.generateIrCommand.extend(inputFiles)
        self.generateIrCommand.extend(self.sharedLibs)
        if not self.noDefaultLibs:
            if self.executable == 'clang':
                self.generateIrCommand.append('-lc')
            elif self.executable == 'clang++':
                self.generateIrCommand.append('-lc')
                self.generateIrCommand.append('-lc++')
            else:
                raise CommandWrapperError('Unsupported linker command: ' + self.executable, self.realCommand)

        # make sure the output file is right at the end so we can see easily which file is being compiled
        self.generateIrCommand.append('-o')
        self.generateIrCommand.append(self.output)

    def parseCommandLine(self):
        # iterate over command line and convert all known options and input files
        skipNextParam = True   # skip the executable
        self.linkCandidates = []
        self.sharedLibs = []
        self.noDefaultLibs = False
        for index, param in enumerate(self.realCommand):
            if skipNextParam:
                skipNextParam = False
                continue
            if param.startswith('-'):
                if param == '-o':
                    if index + 1 >= len(self.realCommand):
                        raise CommandWrapperError('-o flag without parameter!', self.realCommand)
                    skipNextParam = True
                    self.output = correspondingBitcodeName(self.realCommand[index + 1])
                    if '.so.bc' in self.output:
                        self.mode = Mode.shared_lib
                elif param == '-shared':
                    self.mode = Mode.shared_lib
                elif param == '-ffreestanding':
                    self.noDefaultLibs = True
                elif param.startswith('-D' + ENVVAR_NO_EMIT_IR) or param.startswith('-L' + ENVVAR_NO_EMIT_IR):
                    # allow selectively skipping targets by setting this #define or linker search path
                    # e.g. using target_compile_definitions(foo PRIVATE LLVM_IR_WRAPPER_NO_EMIT_LLVM_IR=1)
                    # but since CMake strips -D args from the linker command line you can also use
                    # target_link_libraries(foo PRIVATE -LLLVM_IR_WRAPPER_NO_EMIT_LLVM_IR)
                    self.nothingToDo = True
                    return
                # The only other flag we support is the -l flag, all others are ignored
                # Tell llvm-link to add llvm.sharedlibs metadata
                # TODO: is this the best solution?
                elif param.startswith('-l'):
                    self.sharedLibs.append(param)
                elif param == '-pthread':
                    # the modified llvm-link only understands -l flags
                    self.sharedLibs.append('-lpthread')
                elif param in clangParamsWithArgument():
                    skipNextParam = True
                # ignore all other -XXX flags
                continue
            elif param.endswith('.so') or '.so.' in param or param.endswith('.a') or '.a.' in param:
                # if os.path.isfile(param):
                #     self.linkCandidates.append(param)
                #     continue

                # strip the directory part if it is a path
                filename = os.path.basename(param)
                # remove the leading lib
                if not filename.startswith('lib'):
                    print(infoMsg('Found shared library on command line that doesn\'t start with "lib" -> linking in: ' + param))
                    self.linkCandidates.append(param)
                    continue
                else:
                    # strip the lib part
                    filename = filename[3:]
                filename = correspondingBitcodeName(filename)
                lflag = '-l' + filename
                # print(param, filename, lflag, sep=', ')
                self.sharedLibs.append(lflag)
            else:
                self.linkCandidates.append(param)

        if not self.output:
            print(warningMsg('WARNING: could not determine output file, assuming a.out.bc: ' + quoteCommand(self.realCommand)))
            self.output = 'a.out.bc'
        if self.mode == Mode.unknown:
            self.mode = Mode.executable

        # remove arguments that clang doesn't understand from the link command
        try:
            self.realCommand.remove('-fexcess-precision=standard')
        except:
            pass  # remove throws an exception if the element wasn't found...


class ArWrapper(CommandWrapper):
    def __init__(self, originalCommandLine):
        super().__init__(originalCommandLine)
        self.mode = Mode.static_lib

    def computeWrapperCommand(self):
        if len(self.realCommand) < 2:
            raise CommandWrapperError('Cannot parse AR invocation: ', self.realCommand)
        # Only having 3 parameters means creating an empty .a file (e.g. musl libc uses this)
        self.generateEmptyOutput = len(self.realCommand) == 3
        self.replacement = False
        operation = self.realCommand[1]
        # for now we only understand append 'q' combined with 'c' (create) or 'r'
        if 'r' in operation:
            # replacement, like q, but replaces existing members, hopefully this will work!
            # TODO: use llvm-ar instead? how will that work?
            self.replacement = True
        elif 'x' in operation or 't' in operation or operation == 's':
            # These modes don't need to be wrapped:
            # t   Display a table listing the contents of archive
            # x   Extract members (named member) from the archive.
            # s   Add an index to the archive, or update it if it already exists
            # Note: s can also be a modifier so only skip it if s is the only flag
            self.nothingToDo = True
            return
        elif not ('q' in operation and 'c' in operation):
            raise CommandWrapperError('ar wrapper: \'cq\' or \'r\' mode is currently supported: ', self.realCommand)

        self.output = correspondingBitcodeName(self.realCommand[2])
        self.generateIrCommand = [soaapLlvmBinary('llvm-link'), '-libmd']
        if 'v' in operation:
            self.generateIrCommand.append('-v')
        for file in findBitcodeFiles(self.realCommand[3:]):
            # TODO: --override only replaces existing symbols, it doesn't add new ones.
            # -> --override is useless in this case
            #if self.replacement:
            #    # we add the --override flag to make sure multiple definitions are fine
            #    self.generateIrCommand.append('--override')
            self.generateIrCommand.append(file)
        # make sure the output file is right at the end so we can see easily which file is being compiled
        self.generateIrCommand.append('-o')
        self.generateIrCommand.append(self.output)

    def runGenerateIrCommand(self):
        if self.generateEmptyOutput:
            # we need to create an empty output file (e.g. musl libc creates empty files
            # for libm.a, libcrypt.a, etc since they are all in libc.a)
            with tempfile.NamedTemporaryFile() as tmp:
                # create empty bitcode file and then link it
                self.createEmptyBitcodeFile(tmp.name)
                self.generateIrCommand.append(tmp.name)
                super().runGenerateIrCommand()
        elif self.replacement:
            with tempfile.TemporaryDirectory() as tmpdir:
                movedInput = os.path.join(tmpdir, os.path.basename(self.output))
                # create an empty input file if it doesn't exist yet to make sure llvm-link has the right number of params
                if os.path.isfile(self.output):
                    shutil.move(self.output, movedInput)
                else:
                    self.createEmptyBitcodeFile(movedInput)

                self.generateIrCommand.append(movedInput)
                print('Moved input:', movedInput)
                # input('About to run: ' + str(self.generateIrCommand))
                super().runGenerateIrCommand()
        else:
            # otherwise just call the superclass method
            super().runGenerateIrCommand()


class RanlibWrapper(CommandWrapper):
    def __init__(self, originalCommandLine):
        super().__init__(originalCommandLine)
        self.mode = Mode.ranlib

    def computeWrapperCommand(self):
        self.output = correspondingBitcodeName(self.realCommand[1])
        self.nothingToDo = True


#  check if a file with the name and .bc appended exists, if that fails
#  execute the `file` command to determine whether the file is a bitcode file
def findBitcodeFiles(files):
    found = []
    toTest = []
    missingFiles = []
    for f in files:
        bitcodeName = correspondingBitcodeName(f)
        if os.path.exists(bitcodeName):
            # print('found bitcode file', bitcodeName)
            found.append(bitcodeName)
        # work around libtool moving stuff around ....
        elif bitcodeName.startswith('.libs/'):
            bitcodeName = bitcodeName[len('.libs/'):]
            if os.path.exists(bitcodeName):
                # print('found libtool bitcode file', f, '->', bitcodeName)
                found.append(bitcodeName)
            else:
                toTest.append(str(f))
        else:
            toTest.append(str(f))
    if len(toTest) > 0:
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
                missingFiles.append(testedFile)
    if len(found) == 0:
        print(warningMsg('No bitcode files found from input ' + quoteCommand(files)))
    if len(missingFiles) > 0:
        if os.getenv(ENVVAR_SKIP_MISSING_LINKER_INPUT):
            print(warningMsg('LLVM IR NOT FOUND for: ' + quoteCommand(missingFiles)))
        else:
            raise CommandWrapperError('Missing input files for: ' + str(missingFiles) +
                                      " in " + os.getcwd(), sys.argv)
    return found
