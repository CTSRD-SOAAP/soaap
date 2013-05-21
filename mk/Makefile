.SUFFIXES: .c .bc .cg .soaap

SOAAP_BUILD_DIR=/home/kg365/pool/soaap
SOAAP_INCLUDE_PATH=/home/kg365/workspace/soaap/include
LLVM_BUILD_DIR=/home/kg365/pool/llvm

.bc.soaap:
	opt -load $(SOAAP_BUILD_DIR)/SoaapPass/libsoaap.so -soaap -o /dev/null ${.IMPSRC}

.c.bc:
	clang -c -g -I $(SOAAP_INCLUDE_PATH) -emit-llvm -o ${.TARGET} ${.IMPSRC}

.bc.dyncg:
	opt -load $(SOAAP_BUILD_DIR)/CallEdgeProfiling/libcep.so -insert-call-edge-profiling -o ${.IMPSRC:R}.pbc ${.IMPSRC} 
	llc -filetype=obj -o ${.IMPSRC:R}.po ${.IMPSRC:R}.pbc 
	clang -L $(SOAAP_BUILD_DIR)/CallEdgeProfiling -L $(LLVM_BUILD_DIR)/lib -lcep_rt -lprofile_rt $(LDADD) -o ${.TARGET} ${.IMPSRC:R}.po

clean:
	rm *.soaap *.dyncg *.pbc *.po *.bc
