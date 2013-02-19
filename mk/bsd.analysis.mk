# $FreeBSD$
#
# The include file <bsd.analysis.mk> provides source code analysis tools like
# C static and dynamic call graphs, and SOAAP.
#
# +++ targets +++
#
#	callgraph:
#		Generate a GraphViz .dot file in ${.OBJDIR}/_callgraph_.dot containing
#		the static call graph of all C (and maybe C++) functions.
#
#		The resulting graph may be quite large, so you may like to filter it
#		using scripts from https://github.com/trombonehero/dot-tools.
#

.include <bsd.init.mk>
.include <bsd.llvm.mk>

SOAAP_BUILD_DIR=/home/khilan/workspace_local/soaap_build
LLVM_BUILD_DIR=/home/khilan/workspace_local/llvm_build
CFLAGS+=-I/home/khilan/workspace/soaap/include

.SUFFIXES: .dot .cg .soaap

DOTFILES=  ${LLVM_BC:R:S/$/.dot/g}
CLEANFILES+= ${DOTFILES} _callgraph_.dot _callgraph_.bc *.pll* *.soaap

callgraph: _callgraph_.dot

_callgraph_.bc: ${LLVM_BC}
	llvm-link -o $@ $?

# Extract static call graph from LLVM IR (binary .bc or textual .ll).
.bc.dot:
	opt -analyze -dot-callgraph $<
	mv callgraph.dot $@     # LLVM currently hardcodes the .dot output file
	@echo "====================================================================="
	@echo "Callgraph located at ${.OBJDIR}/$@
	@echo "You may want to filter it using tools from"
	@echo "https://github.com/trombonehero/dot-tools"
	@echo "====================================================================="

.ll.dot:
	opt -analyze -dot-callgraph $<
	mv callgraph.dot $@     # LLVM currently hardcodes the .dot output file

# Instrument LLVM IR with necessary logic for generating a dynamic call graph
.ll.cg:
	opt -S -load $(SOAAP_BUILD_DIR)/libcep.so -insert-call-edge-profiling -o=`basename $< .ll`.pll $<
	llc -filetype=obj `basename $< .ll`.pll
	clang -L $(SOAAP_BUILD_DIR) -L $(LLVM_BUILD_DIR)/lib -lcep_rt -lprofile_rt $(LDADD) -o `basename $< .ll` `basename $< .ll`.pll.o

# Run SOAAP on the LLVM IR (after having generated a dynamic call graph)
.ll.soaap: 
	opt -S -load $(SOAAP_BUILD_DIR)/libsoaap.so -profile-loader -soaap -o $<.soaap $<

