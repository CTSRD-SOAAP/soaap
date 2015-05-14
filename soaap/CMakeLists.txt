# main soaap pass
add_llvm_library(SOAAP
  Passes/Soaap.cpp
  Common/CmdLineOpts.cpp
  Common/Debug.cpp
  Common/Sandbox.cpp
  Common/XO.cpp
  Analysis/VulnerabilityAnalysis.cpp
  Analysis/PrivilegedCallAnalysis.cpp
  Analysis/SandboxedFuncAnalysis.cpp
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
  OS/LinuxSysCallProvider.cpp
  OS/SysCallProvider.cpp
  OS/Sandbox/NoSandboxPlatform.cpp
  OS/Sandbox/Capsicum.cpp
  OS/Sandbox/SandboxPlatform.cpp
  OS/Sandbox/Seccomp.cpp
  OS/Sandbox/SeccompBPF.cpp
  Util/CallGraphUtils.cpp
  Util/ClassHierarchyUtils.cpp
  Util/ContextUtils.cpp
  Util/DebugUtils.cpp
  Util/LLVMAnalyses.cpp
  Util/PrettyPrinters.cpp
  Util/SandboxUtils.cpp
  Util/ClassifiedUtils.cpp
  Util/TypeUtils.cpp
  Util/InstUtils.cpp
)

if (TARGET libxo)
  add_dependencies(SOAAP libxo)
endif()

target_link_libraries(SOAAP xo)
