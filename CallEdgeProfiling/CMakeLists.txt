add_llvm_loadable_module(libcep CallEdgeProfiling.cpp CallEdgeProfiling.c)

link_directories(${LLVM_BINARY_DIR}/lib)
add_library(cep_rt SHARED CallEdgeProfiling.c)
target_link_libraries(cep_rt profile_rt)
