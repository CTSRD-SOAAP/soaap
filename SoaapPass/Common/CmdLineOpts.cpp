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

string CmdLineOpts::DumpVirtualCallees;
static cl::opt<string, true> ClDumpVirtualCallees("soaap-dump-virtual-callees",
       cl::desc("Dump C++ virtual callees (derived from debugging information) to file"),
       cl::value_desc("filename"),
       cl::location(CmdLineOpts::DumpVirtualCallees));

string CmdLineOpts::ReadVirtualCallees;
static cl::opt<string, true> ClReadVirtualCallees("soaap-read-virtual-callees",
       cl::desc("Read C++ virtual callees from file"),
       cl::value_desc("filename"),
       cl::location(CmdLineOpts::ReadVirtualCallees));
