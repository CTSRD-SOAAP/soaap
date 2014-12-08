# main soaap pass
add_llvm_library(SOAAP
  Passes/Soaap.cpp
  Common/CmdLineOpts.cpp
  Common/Debug.cpp
  Common/Sandbox.cpp
  Analysis/VulnerabilityAnalysis.cpp
  Analysis/PrivilegedCallAnalysis.cpp
  Analysis/CFGFlow/GlobalVariableAnalysis.cpp
  Analysis/CFGFlow/SysCallsAnalysis.cpp
  Analysis/InfoFlow/AccessOriginAnalysis.cpp
  Analysis/InfoFlow/CapabilitySysCallsAnalysis.cpp
  Analysis/InfoFlow/SandboxPrivateAnalysis.cpp
  Analysis/InfoFlow/ClassifiedAnalysis.cpp
  Analysis/InfoFlow/CapabilityAnalysis.cpp
  Analysis/InfoFlow/DeclassifierAnalysis.cpp
  Analysis/InfoFlow/FPAnnotatedTargetsAnalysis.cpp
  Analysis/InfoFlow/FPInferredTargetsAnalysis.cpp
  Analysis/InfoFlow/FPTargetsAnalysis.cpp
  Analysis/InfoFlow/RPC/RPCGraph.cpp
  Instrument/PerformanceEmulationInstrumenter.cpp
  OS/FreeBSDSysCallProvider.cpp
  OS/SysCallProvider.cpp
  OS/Sandbox/NoSandboxPlatform.cpp
  OS/Sandbox/Capsicum.cpp
  OS/Sandbox/SandboxPlatform.cpp
  OS/Sandbox/Seccomp.cpp
  Report/IR/CallGraph.cpp
  Report/IR/Report.cpp
  Report/IR/SourceLocation.cpp
  Report/IR/Vulnerability.cpp
  Report/Render/ConsoleRenderer.cpp
  Report/Render/JSONRenderer.cpp
  Util/CallGraphUtils.cpp
  Util/ClassHierarchyUtils.cpp
  Util/ContextUtils.cpp
  Util/DebugUtils.cpp
  Util/LLVMAnalyses.cpp
  Util/PrettyPrinters.cpp
  Util/SandboxUtils.cpp
  Util/ClassifiedUtils.cpp
  Util/TypeUtils.cpp
)

# pass to output dynamic callgraph to DOT
#add_llvm_loadable_module(libdotdyncg
#  Passes/DOTDynCG.cpp
#)

# pass to instrument dynamic call edge profiling
#add_llvm_loadable_module(libcep
#  Passes/CallEdgeProfiling.cpp
#  Passes/CallEdgeProfiling.c
#)
#link_directories(${LLVM_BINARY_DIR}/lib)
#add_library(cep_rt SHARED Passes/CallEdgeProfiling.c)
#target_link_libraries(cep_rt profile_rt)
