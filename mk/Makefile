.SUFFIXES: .c .bc .soaap .soaap_cg .soaap_perf

SOAAP_BUILD_DIR=/home/khilan/workspace_local/soaap_build
SOAAP_INCLUDE_PATH=/home/khilan/workspace/soaap/include
LLVM_BUILD_DIR=/home/khilan/workspace_local/llvm_build

.c.bc:
	clang -c -g -I $(SOAAP_INCLUDE_PATH) -emit-llvm -o ${.TARGET} ${.IMPSRC}

.bc.soaap:
	opt -load $(SOAAP_BUILD_DIR)/libsoaap.so -soaap ${SOAAP_FLAGS} -o /dev/null ${.IMPSRC}

.bc.soaap_cg:
	opt -load $(SOAAP_BUILD_DIR)/libcep.so -insert-call-edge-profiling -o ${.IMPSRC:R}.pbc ${.IMPSRC} 
	llc -filetype=obj -o ${.IMPSRC:R}.po ${.IMPSRC:R}.pbc 
	clang -L $(SOAAP_BUILD_DIR) -L $(LLVM_BUILD_DIR)/lib -lcep_rt -lprofile_rt $(LDADD) -o ${.TARGET} ${.IMPSRC:R}.po

.bc.soaap_perf:
	opt -load $(SOAAP_BUILD_DIR)/libsoaap.so -soaap -soaap-emulate-performance ${SOAAP_FLAGS} -o ${.IMPSRC:R}.pbc ${.IMPSRC}
	llc -filetype=obj -o ${.IMPSRC:R}.po ${.IMPSRC:R}.pbc 
	clang -o ${.TARGET} ${.IMPSRC:R}.po

clean:
	rm *.soaap *.soaap_cg *.soaap_perf *.pbc *.po *.bc
