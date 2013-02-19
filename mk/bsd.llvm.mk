# $FreeBSD$
#
# The include file <bsd.llvm.mk> handles LLVM-specific tasks like compiling
# C to LLVM IR.
#
# +++ targets +++
#
#	bc:
#		compile all C sources in ${SRCS} to binary LLVM IR format
#
#	ir:
#		compile all C sources in ${SRCS} to textual LLVM IR format
#
#       all_llvm:
#		compile all C sources in ${SRCS} to textual LLVM IR format
#		and also process all non-C files (i.e. generate headers and
#		compile ASM files to object files). Also, links all resulting
#		LLVM IR files into a single .ll file, if this is a library.
#

.include <bsd.init.mk>

.SUFFIXES: .c .cc .cpp .cxx .C .bc .ll 

# LLVM bytecode is a binary format.
LLVM_BC=     ${SRCS:M*.c*:R:S/$/.bc/}

# LLVM IR contains the same information, but in an assembly-like format.
LLVM_IR=     ${SRCS:M*.c*:R:S/$/.ll/}

# Generated headers and ASM files
OTHERFILES=     ${SRCS:N*.c}

CLEANFILES+= ${LLVM_BC} ${LLVM_IR} ${OTHERFILES} 
.ifdef LIB
CLEANFILES+= lib$(LIB).ll
.endif

# Build all LLVM IR (binary .bc or textual .ll).
bc: ${LLVM_BC}
ir: ${LLVM_IR}

# Build all non-C files (Generated headers and ASM)
other: ${OTHERFILES}

# Link all LLVM IR files into a single file (if a library)
irlink: ir
.ifdef LIB
	llvm-link -o=lib$(LIB).ll ${LLVM_IR}
.endif

all_llvm: other ir irlink

# Compile C code to binary LLVM IR bytecode.
.c.bc: usingclang
.if defined(PROG_CXX)
	clang++ -c -g -emit-llvm ${CFLAGS} -o ${@:T} $<
.else
	clang -c -g -emit-llvm ${CFLAGS} -o ${@:T} $<
.endif

# Compile C code to textual LLVM IR format.
.c.ll: usingclang
.if defined(PROG_CXX)
	clang++ -S -g -emit-llvm ${CFLAGS} -o ${@:T} $<
.else
	clang -S -g -emit-llvm ${CFLAGS} -o ${@:T} $<
.endif

# Ensure that we are compiling with clang.
usingclang:
.if ${MK_CLANG} != "yes"
	@echo "===================================================================="
	@echo "C -> LLVM IR compilation requires clang."
	@echo "You may wish to enable WITH_CLANG in /etc/make.conf."
	@echo "===================================================================="
	@exit 1
.endif

