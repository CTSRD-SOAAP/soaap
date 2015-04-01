# Compiler wrappers for generating LLVM bitcode

The scripts in this directory allow generating LLVM IR from projects without
directly modifying their build system. This is done by wrapping the compiler
linker or ar invocation with a python script that first generates the IR file
and then performs the actual compiler command. While this takes approximately
twice as long it is the only solution that works in cases where binaries are
created during the build and then run to generate other files (e.g. moc, uic
from the qtbase build system)

## Supported build systems

Currently the following build systems have been tested:

- CMake
- QMake (including the qtbase configure script)
- Autotools (openSSH)
- Custom Makefiles (as long as they use common variable names)


## Instructions:

For most build systems it is enough to do the following:

```
    # export CC=<script-dir>/clang-and-emit-llvm-ir.py
    # export CXX==<script-dir>/clang-and-emit-llvm-ir.py
    # export NO_EMIT_LLVM_IR=1
    # ./configure or cmake or qmake
    # unset NO_EMIT_LLVM_IR
    # make -j8
```

Exporting `NO_EMIT_LLVM_IR` during the configure step is very important since
during the configure step the compiler output is often parsed. Since the
additional IR generation step (and debug output) will interfere with this many
compiler features will not be detected correctly.

## Current limitations

- Any build system that uses `libtool` does not work since libtool is a weird shell
script that moves files around and deletes the generated IR files.
- Build systems that directly invoke the linker instead of using the compiler for
the linking step do not work yet

