add_llvm_loadable_module(libsoaap
  SoaapPass.cpp
  Analysis/InfoFlow/AccessOriginAnalysis.cpp
  Analysis/InfoFlow/InfoFlowAnalysis.cpp
  Utils/LLVMAnalyses.cpp
  Utils/PrettyPrinters.cpp
)
