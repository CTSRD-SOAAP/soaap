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


class LinkerWrapper(CommandWrapper):

    def __init__(self, originalCommandLine):
        super().__init__(originalCommandLine)

    def computeWrapperCommand(self):
        if self.executable in ('ld', 'gold', 'lld') or self.executable.startswith('ld.'):
            raise NotImplementedError('Wrapping direct ld calls not supported yet!')
        else:
            assert self.executable.startswith('clang')

        self.parseCommandLine()

        if len(self.linkCandidates) == 0:
            raise RuntimeError('NO LINK CANDIDATES FOUND IN CMDLINE: ', self.realCommand)

        self.generateIrCommand = [soaapLlvmBinary('llvm-link'), '-o', self.output]

        inputFiles = findBitcodeFiles(self.linkCandidates)
        if len(inputFiles) == 0:
            raise RuntimeError('NO FILES FOUND FOR LINKING!', self.realCommand)
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
                raise RuntimeError('Unsupported linker command: ' + self.executable)

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
                        raise RuntimeError('-o flag without parameter!')
                    skipNextParam = True
                    self.output = correspondingBitcodeName(self.realCommand[index + 1])
                    if '.so.bc' in self.output:
                        self.mode = Mode.shared_lib
                elif param == '-shared':
                    self.mode = Mode.shared_lib
                elif param == '-ffreestanding':
                    self.noDefaultLibs = True
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
            elif param.endswith('.so') or '.so.' in param:
                self.sharedLibs.append(param)
            else:
                self.linkCandidates.append(param)

        if not self.output:
            print(warningMsg('WARNING: could not determine output file, assuming a.out.bc: ' + str(self.realCommand)))
            self.output = 'a.out.bc'
        if self.mode == Mode.unknown:
            self.mode = Mode.executable

        # remove arguments that clang doesn't understand from the link command
        try:
            self.realCommand.remove('-fexcess-precision=standard')
        except:
            pass # remove throws an exception if the element wasn't found...


class ArWrapper(CommandWrapper):
    def __init__(self, originalCommandLine):
        super().__init__(originalCommandLine)
        self.mode = Mode.static_lib

    def computeWrapperCommand(self):
        if len(self.realCommand) < 2:
            raise RuntimeError('Cannot parse AR invocation: ', self.realCommand)
        # Only having 3 parameters means creating an empty .a file (e.g. musl libc uses this)
        self.emptyOutputFile = len(self.realCommand) == 3
        operation = self.realCommand[1]
        # for now we only understand append 'q' combined with 'c' (create)
        if 'r' in operation:
            # replacement, like q, but replaces existing members, hopefully this will work!
            # TODO: use llvm-ar instead? how will that work?
            pass
        elif not ('q' in operation and 'c' in operation):
            raise RuntimeError('ar wrapper: \'cq\' or \'r\' mode is currently supported: ', self.realCommand)

        self.output = correspondingBitcodeName(self.realCommand[2])
        self.generateIrCommand = [soaapLlvmBinary('llvm-link'), '-o', self.output]
        self.generateIrCommand.extend(findBitcodeFiles(self.realCommand[3:]))

    def runGenerateIrCommand(self):
        if not self.emptyOutputFile:
            super().runGenerateIrCommand()
            return
        # we need to create an empty output file (e.g. musl libc creates empty files
        # for libm.a, libcrypt.a, etc since they are all in libc.a)
        with tempfile.NamedTemporaryFile() as tmp:
            # create empty bitcode file and then link it
            subprocess.check_call([soaapLlvmBinary('clang'), '-c', '-emit-llvm', '-o', tmp.name,
                                   '-x', 'c', '/dev/null'])
            self.generateIrCommand.append(tmp.name)
            subprocess.check_call(self.generateIrCommand)


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
                print(warningMsg('LLVM IR NOT FOUND for: ' + testedFile))
    if len(found) == 0:
        print(warningMsg('No bitcode files found from input ' + str(files)))

    return found
