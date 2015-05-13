# linux-pam

`configure-for-llvm-ir.py --ld=ld --enable-static-modules && make`

# zlib

Apply patch:

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

Then `configure-for-llvm-ir.py && make`

# qtbase

`configure-for-llvm-ir.py --cpp-linker <options> (-nomake tests -nomake examples) && make`

This requires a recent branch containing git commit f041757d7a9e76c8463609d8169339b4b0dae3f6 or
alternatively manually applying the patch from https://codereview.qt-project.org/#/c/109807/

# openssh

`configure-for-llvm-ir.py && make`

# krb5

We need a symlink for libss.a.bc since otherwise it isn't found when linking

`git clone https://github.com/krb5/krb5.git`
`cd krb5/src/lib && ln -s ../util/ss/libss.a.bc . && cd ..`
`configure-for-llvm-ir.py --without-libedit && make`

# openssl
`configure-for-llvm-ir.py -f ./config no-asm zlib shared && make`

The option "no-asm" is very important since otherwise it attempts to compile assembly .s files which won't work.
Also make sure to **run make with only one job** since otherwise libcrypto.a.bc might be corrupted

If there are any errors due to multiple definitions from llvm-link make sure you delete `libcrypto.a.bc`, run `make clean`
and then run `make` again. Or alternatively you can do `git clean -dfx` to start with a clean directory.

# musl libc

`configure-for-llvm-ir.py && make`


# linux-pam

`./autogen.sh`
`configure-for-llvm-ir.py --enable-static-modules && make-for-llvm-ir.py`

# Almost every CMake project:

`mkdir -p build && cd build && cmake-for-llvm-ir.py .. && make`
