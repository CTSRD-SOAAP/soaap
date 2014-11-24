.SUFFIXES: .c .cc .dbc .dll .bc .ll .dbcsoaap .soaap .soaap_cg .soaap_perf

.c.bc:
	${LLVM_BUILD_DIR}/bin/clang -c -g -I $(SOAAP_SOURCE_DIR)/include -emit-llvm -o ${.TARGET} ${.IMPSRC}

.cc.bc:
	${LLVM_BUILD_DIR}/bin/clang++ -c -g -I $(SOAAP_SOURCE_DIR)/include -emit-llvm -o ${.TARGET} ${.IMPSRC}

.bc.ll:
	${LLVM_BUILD_DIR}/bin/llvm-dis -o ${.TARGET} ${.IMPSRC}

.dbc.dll:
	${LLVM_BUILD_DIR}/bin/llvm-dis -o ${.TARGET} ${.IMPSRC}

.bc.soaap:
	${SOAAP_BUILD_DIR}/bin/soaap ${SOAAP_FLAGS} -o /dev/null ${.IMPSRC}

.dbc.dbcsoaap:
	${SOAAP_BUILD_DIR}/bin/soaap ${SOAAP_FLAGS} -o /dev/null ${.IMPSRC}

.bc.dbc:
	${LLVM_BUILD_DIR}/bin/opt -load $(SOAAP_BUILD_DIR)/libclassdebuginfo.so -soaap-insert-class-debug-info ${SOAAP_FLAGS} -o ${.TARGET} ${.IMPSRC}

.bc.soaap_perf:
	${SOAAP_BUILD_DIR}/bin/soaap -soaap-emulate-performance ${SOAAP_FLAGS} -o ${.IMPSRC:R}.pbc ${.IMPSRC}
	${LLVM_BUILD_DIR}/bin/llc -filetype=obj -o ${.IMPSRC:R}.po ${.IMPSRC:R}.pbc 
	${LLVM_BUILD_DIR}/bin/clang -o ${.TARGET} ${.IMPSRC:R}.po

clean:
	rm *.soaap *.soaap_cg *.soaap_perf *.pbc *.po *.bc *.dbc
