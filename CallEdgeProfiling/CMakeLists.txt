add_llvm_loadable_module(libcep CallEdgeProfiling.cpp CallEdgeProfiling.c)
add_library(cep_rt SHARED CallEdgeProfiling.c)
