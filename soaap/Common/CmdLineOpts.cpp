#include "Common/CmdLineOpts.h"

using namespace soaap;

list<string> CmdLineOpts::VulnerableVendors;
static cl::list<string, list<string> > ClVulnerableVendors("soaap-vulnerable-vendors",
       cl::desc("Comma-separated list of vendors whose code should "
                "be treated as vulnerable"),
       cl::value_desc("list of vendors"),
       cl::CommaSeparated,
       cl::location(CmdLineOpts::VulnerableVendors));

bool CmdLineOpts::EmPerf;
static cl::opt<bool, true> ClEmPerf("soaap-emulate-performance",
       cl::desc("Emulate sandboxing performance"),
       cl::location(CmdLineOpts::EmPerf));

bool CmdLineOpts::ContextInsens;
static cl::opt<bool, true> ClContextInsens("soaap-context-insens",
       cl::desc("Don't use context-sensitive analysis"),
       cl::location(CmdLineOpts::ContextInsens));

bool CmdLineOpts::ListSandboxedFuncs;
static cl::opt<bool, true> ClListSandboxedFuncs("soaap-list-sandboxed-funcs",
       cl::desc("List sandboxed functions"),
       cl::location(CmdLineOpts::ListSandboxedFuncs));

bool CmdLineOpts::ListFPCalls;
static cl::opt<bool, true> ClListFPCalls("soaap-list-fp-calls",
       cl::desc("List function-pointer calls"),
       cl::location(CmdLineOpts::ListFPCalls));

bool CmdLineOpts::InferFPTargets;
static cl::opt<bool, true> ClInferFPTargets("soaap-infer-fp-targets",
       cl::desc("Infer function-pointer targets by tracking assignments"),
       cl::location(CmdLineOpts::InferFPTargets));

bool CmdLineOpts::ListFPTargets;
static cl::opt<bool, true> ClListFPTargets("soaap-list-fp-targets",
       cl::desc("List function-pointer targets"),
       cl::location(CmdLineOpts::ListFPTargets));

bool CmdLineOpts::ListAllFuncs;
static cl::opt<bool, true> ClListAllFuncs("soaap-list-all-funcs",
       cl::desc("List all functions"),
       cl::location(CmdLineOpts::ListAllFuncs));

bool CmdLineOpts::Pedantic;
static cl::opt<bool, true> ClPedantic("soaap-pedantic",
       cl::desc("Output all warnings"),
       cl::location(CmdLineOpts::Pedantic));

#ifndef NDEBUG
string CmdLineOpts::DebugModule;
static cl::opt<string, true> ClDebugModule("soaap-debug-module",
       cl::desc("Output debug info for the specified SOAAP module"),
       cl::location(CmdLineOpts::DebugModule));

int CmdLineOpts::DebugVerbosity;
static cl::opt<int, true> ClDebugVerbosity("soaap-debug-verbosity",
       cl::desc("Level of debug verbosity"),
       cl::location(CmdLineOpts::DebugVerbosity));
#endif
