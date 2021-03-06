# linux-pam

`configure-for-llvm-ir.py --ld=ld --enable-static-modules && make`

# zlib

`configure-for-llvm-ir.py && make-for-llvm-ir.py`

Instead of the wrapper script you can also apply this patch and use normal `make` instead (i.e. `configure-for-llvm-ir.py && make`)

```
diff --git a/Makefile.in b/Makefile.in
index c61aa30..c9d2771 100644
--- a/Makefile.in
+++ b/Makefile.in
@@ -158,6 +158,7 @@ minigzip64.o: test/minigzip.c zlib.h zconf.h
        -@mkdir objs 2>/dev/null || test -d objs
        $(CC) $(SFLAGS) -DPIC -c -o objs/$*.o $<
        -@mv objs/$*.o $@
+       -@mv objs/$*.o.bc $@.bc

 placebo $(SHAREDLIBV): $(PIC_OBJS) libz.a
        $(LDSHARED) $(SFLAGS) -o $@ $(PIC_OBJS) $(LDSHAREDLIBC) $(LDFLAGS)

```

# qtbase

`configure-for-llvm-ir.py --cpp-linker <options> (-nomake tests -nomake examples)`
`make` or `make-for-llvm-ir.py '--ar=ar cqs'`

This requires a recent branch containing git commit f041757d7a9e76c8463609d8169339b4b0dae3f6 or
alternatively manually applying the patch from https://codereview.qt-project.org/#/c/109807/

# openssh

`configure-for-llvm-ir.py && make`

# krb5

We need a symlink for libss.a.bc since otherwise it isn't found when linking

`git clone https://github.com/krb5/krb5.git`
`cd krb5/src/lib && ln -s ../util/ss/libss.a.bc . && cd ..`
`autoreconf && configure-for-llvm-ir.py --without-libedit && make-for-llvm-ir.py`

# openssl
`configure-for-llvm-ir.py -f ./config no-asm zlib shared && make`

The option "no-asm" is very important since otherwise it attempts to compile assembly .s files which won't work.
Also make sure to **run make with only one job** since otherwise libcrypto.a.bc might be corrupted

If there are any errors due to multiple definitions from llvm-link make sure you delete `libcrypto.a.bc`, run `make clean`
and then run `make` again. Or alternatively you can do `git clean -dfx` to start with a clean directory.

# musl libc

`configure-for-llvm-ir.py && make`

# libcxx

`mkdir build && cd build && cmake-for-llvm-ir.py .. && ninja`

Or optionally build libcxxabi first:
`cd $PATH_TO_LIBCXXABI && mkdir build && cd build && cmake-for-llvm-ir.py .. && ninja`
`cd $PATH_TO_LIBCXX && mkdir build && cd build`
`cmake-for-llvm-ir.py -DLIBCXX_CXX_ABI=libcxxabi -DLIBCXX_CXX_ABI_INCLUDE_PATHS=$PATH_TO_LIBCXXABI/include/ .. && ninja`



# linux-pam

`./autogen.sh`
`configure-for-llvm-ir.py --enable-static-modules && make-for-llvm-ir.py`

# Almost every CMake project:

`mkdir -p build && cd build && cmake-for-llvm-ir.py .. && make`
