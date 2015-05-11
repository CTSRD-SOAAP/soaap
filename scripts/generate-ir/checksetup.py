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
import os
# make sure we have enum and termcolor
from enum import Enum
import termcolor

#  TODO: let cmake set this at configure time
SOAAP_LLVM_BINDIR = os.getenv('SOAAP_LLVM_BINDIR', os.path.expanduser('~') + '/devel/soaap/llvm/build/bin/')
if not os.path.isdir(SOAAP_LLVM_BINDIR):
    sys.exit('could not find SOAAP_LLVM_BINDIR, please make sure the env var is set correctly')
