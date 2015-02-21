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

string CmdLineOpts::DebugModule;
static cl::opt<string, true> ClDebugModule("soaap-debug-module",
       cl::desc("Output debug info for the specified SOAAP module pattern"),
       cl::location(CmdLineOpts::DebugModule));

string CmdLineOpts::DebugFunction;
static cl::opt<string, true> ClDebugFunction("soaap-debug-function",
       cl::desc("Only output debug info for the specified SOAAP function pattern"),
       cl::location(CmdLineOpts::DebugFunction));

int CmdLineOpts::DebugVerbosity;
static cl::opt<int, true> ClDebugVerbosity("soaap-debug-verbosity",
       cl::desc("Level of debug verbosity"),
       cl::location(CmdLineOpts::DebugVerbosity));

int CmdLineOpts::SummariseTraces;
static cl::opt<int, true> ClSummariseTraces("soaap-summarise-traces",
       cl::desc("Summarise stack traces so that atmost the specified number of calls are shown from the top and the same number from the bottom of the trace"),
       cl::location(CmdLineOpts::SummariseTraces));

bool CmdLineOpts::DumpRPCGraph;
static cl::opt<bool, true> ClDumpRPCGraph("soaap-dump-rpc-graph",
       cl::desc("Dump RPC Graph"),
       cl::location(CmdLineOpts::DumpRPCGraph));

SandboxPlatformName CmdLineOpts::SandboxPlatform;
static cl::opt<SandboxPlatformName, true> ClSandboxPlatform("soaap-sandbox-platform",
       cl::desc("Sandbox platform to model"),
       cl::values(
         clEnumValN(SandboxPlatformName::None, "none", "None"),
         clEnumValN(Annotated, "annotated", "Annotated"),
         clEnumValN(Capsicum, "capsicum", "Capsicum (default)"),
         clEnumValN(Seccomp, "seccomp", "Secure Computing Mode (Seccomp)"),
       clEnumValEnd),
       cl::location(CmdLineOpts::SandboxPlatform),
       cl::init(Capsicum)); // default value is Capsicum

bool CmdLineOpts::DumpDOTCallGraph;
static cl::opt<bool, true> ClDumpDOTCallGraph("soaap-dump-dot-callgraph",
       cl::desc("Dump DOT CallGraph"),
       cl::location(CmdLineOpts::DumpDOTCallGraph));

bool CmdLineOpts::PrintCallGraph;
static cl::opt<bool, true> ClPrintCallGraph("soaap-print-callgraph",
       cl::desc("Print CallGraph"),
       cl::location(CmdLineOpts::PrintCallGraph));

list<ReportOutputFormat> CmdLineOpts::ReportOutputFormats;
static cl::list<ReportOutputFormat, list<ReportOutputFormat> > ClReportOutputFormats("soaap-report-output-formats",
       cl::desc("Comma-separated list of report-output formats"),
       cl::value_desc("list of report-output formats"),
       cl::values(
         clEnumValN(Text, "text", "Text (on stdout)"),
         clEnumValN(JSON, "json", "JSON"),
         clEnumValN(XML, "xml", "XML"),
         clEnumValN(HTML, "html", "HTML"),
       clEnumValEnd),
       cl::CommaSeparated,
       cl::location(CmdLineOpts::ReportOutputFormats));

string CmdLineOpts::ReportFilePrefix;
static cl::opt<string, true> ClReportFilePrefix("soaap-report-file-prefix",
       cl::desc("Prefix for report-output filenames (default is "
                "\"soaap-output\")"),
       cl::location(CmdLineOpts::ReportFilePrefix),
       cl::init("soaap-output"));

bool CmdLineOpts::SysCallTraces;
static cl::opt<bool, true> ClSysCallTraces("soaap-syscall-traces",
       cl::desc("Show traces for system call warnings"),
       cl::location(CmdLineOpts::SysCallTraces));
