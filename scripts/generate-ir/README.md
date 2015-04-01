# Compiler wrappers for generating LLVM bitcode

The scripts in this directory allow generating LLVM IR from projects without
directly modifying their build system. This is done by wrapping the compiler
linker or ar invocation with a python script that first generates the IR file
and then performs the actual compiler command. While this takes approximately
twice as long it is the only solution that works in cases where binaries are
created during the build and then run to generate other files (e.g. moc, uic
from the qtbase build system)

## Generated files:

For every object file `foo.o` it will generated a corresponding LLVM bitcode
file `foo.o.bc`. Every static library `foo.a` will also have a corresponding
`foo.a.bc` that contains all linked object files.

Shared libraries (`foo.so` -> `foo.so.bc`) and executables (`foo` -> `foo.bc`)
will only contain the linked object files as well as any static libraries that
were passed on the link command line. Any `.so` files as well as `-lfoo` passed
to the linker will not be included since this can result in duplicate symbols
which will cause `llvm-link` to fail.

Before running a SOAAP analysis on the generated binaries you should run
`# llvm-nm foo | grep 'U '` to see whether there are any important unresolved
symbols.

## Supported build systems

Currently the following build systems have been tested:

- CMake
- QMake (excluding qtbase)
- Autotools (openSSH)
- Custom Makefiles (as long as they use common variable names)


## Instructions:
**Important**: Currently you must set`SOAAP_LLVM_BINDIR` to point to the
LLVM build directory (`$SOAAP_LLVM_BINDIR/clang` must exist)

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

If the build system uses hand written Makefiles or the configure step does not
set the compiler wrappers for some reason there is also a script that wraps `make`.
In that case you can run and hopefully everything will work

```
    # make-for-llvm-ir.py <make-options>.
```

This method has been tested with the qtbase build system and the openSSH build system.

**Note**: this has only been tested with **GNU make**.

## Current limitations

- Any build system that uses `libtool` does not work since libtool is a weird shell
script that moves files around and deletes the generated IR files.
- Build systems that directly invoke the linker instead of using the compiler for
the linking step do not work yet
- Symbols for any shared libraries are not included in the resulting binaries yet.
They need to be added manually if whole-program-analysis is desired.
In the future some LLVM metadata might be added to solve this issue

## Urgent TODO:
- if `SOAAP_LLVM_BINDIR` is not set it will assume that the LLVM binaries are located at
 /home/alex/devel/soaap/llvm/release-build/bin/
