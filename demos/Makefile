.SUFFIXES: .ll .cg .soaap

.ll.soaap:
	opt -S -o $<.soaap -load $(SOAAP_BUILD_DIR)/libsoaap.so -profile-loader -soaap $<

.c.cg:
	clang -cc1 -g -I $(SOAAP_INCLUDE_PATH) -emit-llvm $<
	opt -S -load $(SOAAP_BUILD_DIR)/libcep.so -insert-call-edge-profiling -o=`basename $< .c`.pll `basename $< .c`.ll
	llc -filetype=obj `basename $< .c`.pll
	clang -L $(SOAAP_BUILD_DIR) -L $(LLVM_BUILD_DIR)/lib -lcep_rt -lprofile_rt -o `basename $< .c` `basename $< .c`.pll.o

clean:
	rm *.pll* *.soaap
