.SUFFIXES: .c .bc .ll .soaap .soaap_cg .soaap_perf

SOAAP_BUILD_DIR=/home/kg365/pool/soaap
SOAAP_INCLUDE_PATH=/home/kg365/workspace/soaap/include
LLVM_BUILD_DIR=/home/kg365/pool/llvm

.c.bc:
	${LLVM_BUILD_DIR}/bin/clang -c -gline-tables-only -I $(SOAAP_INCLUDE_PATH) -emit-llvm -o ${.TARGET} ${.IMPSRC}

.bc.ll:
	${LLVM_BUILD_DIR}/bin/llvm-dis -o ${.TARGET} ${.IMPSRC}

.bc.soaap:
	${LLVM_BUILD_DIR}/bin/opt -analyze -disable-verify -load $(SOAAP_BUILD_DIR)/libsoaap.so -soaap ${SOAAP_FLAGS} -o /dev/null ${.IMPSRC}

.bc.soaap_cg:
	${LLVM_BUILD_DIR}/bin/opt -load $(SOAAP_BUILD_DIR)/libcep.so -insert-call-edge-profiling -o ${.IMPSRC:R}.pbc ${.IMPSRC} 
	${LLVM_BUILD_DIR}/bin/llc -filetype=obj -o ${.IMPSRC:R}.po ${.IMPSRC:R}.pbc 
	${LLVM_BUILD_DIR}/bin/clang -L $(SOAAP_BUILD_DIR) -L $(LLVM_BUILD_DIR)/lib -lcep_rt -lprofile_rt $(LDADD) -o ${.TARGET} ${.IMPSRC:R}.po

.bc.soaap_perf:
	${LLVM_BUILD_DIR}/bin/opt -load $(SOAAP_BUILD_DIR)/libsoaap.so -soaap -soaap-emulate-performance ${SOAAP_FLAGS} -o ${.IMPSRC:R}.pbc ${.IMPSRC}
	${LLVM_BUILD_DIR}/bin/llc -filetype=obj -o ${.IMPSRC:R}.po ${.IMPSRC:R}.pbc 
	${LLVM_BUILD_DIR}/bin/clang -o ${.TARGET} ${.IMPSRC:R}.po

clean:
	rm *.soaap *.soaap_cg *.soaap_perf *.pbc *.po *.bc
