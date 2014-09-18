add_llvm_executable(soaap
  soaap.cpp
)

llvm_map_components_to_libnames(LLVM_LIBS
  Analysis
  BitWriter
  Codegen
  Core
  IPA
  IPO
  IRReader
  InstCombine
  Instrumentation
  MC
  ObjCARCOpts
  ScalarOpts
  Support
  Target
  TransformUtils
  Vectorize
)                                                                                                   

target_link_libraries(soaap ${LLVM_LIBS} SOAAP)
