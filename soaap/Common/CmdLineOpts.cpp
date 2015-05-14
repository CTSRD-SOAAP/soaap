#include "Common/CmdLineOpts.h"

using namespace soaap;

cl::OptionCategory CmdLineOpts::SoaapCategory("SOAAP");

list<string> CmdLineOpts::VulnerableVendors;
static cl::list<string, list<string> > ClVulnerableVendors("soaap-vulnerable-vendors",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Comma-separated list of vendors whose code should "
                "be treated as vulnerable"),
       cl::value_desc("list of vendors"),
       cl::CommaSeparated,
       cl::location(CmdLineOpts::VulnerableVendors));

list<string> CmdLineOpts::VulnerableLibs;
static cl::list<string, list<string> > ClVulnerableLibs("soaap-vulnerable-libs",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Comma-separated list of libraries that should "
                "be treated as vulnerable"),
       cl::value_desc("list of libraries"),
       cl::CommaSeparated,
       cl::location(CmdLineOpts::VulnerableLibs));

bool CmdLineOpts::EmPerf;
static cl::opt<bool, true> ClEmPerf("soaap-emulate-performance",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Emulate sandboxing performance"),
       cl::location(CmdLineOpts::EmPerf));

bool CmdLineOpts::ContextInsens;
static cl::opt<bool, true> ClContextInsens("soaap-context-insens",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Don't use context-sensitive analysis"),
       cl::location(CmdLineOpts::ContextInsens));

bool CmdLineOpts::ListSandboxedFuncs;
static cl::opt<bool, true> ClListSandboxedFuncs("soaap-list-sandboxed-funcs",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("List sandboxed functions"),
       cl::location(CmdLineOpts::ListSandboxedFuncs));

bool CmdLineOpts::ListPrivilegedFuncs;
static cl::opt<bool, true> ClListPrivilegedFuncs("soaap-list-priv-funcs",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("List privileged functions"),
       cl::location(CmdLineOpts::ListPrivilegedFuncs));

bool CmdLineOpts::ListFPCalls;
static cl::opt<bool, true> ClListFPCalls("soaap-list-fp-calls",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("List function-pointer calls"),
       cl::location(CmdLineOpts::ListFPCalls));

bool CmdLineOpts::InferFPTargets;
static cl::opt<bool, true> ClInferFPTargets("soaap-infer-fp-targets",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Infer function-pointer targets by tracking assignments"),
       cl::location(CmdLineOpts::InferFPTargets));

bool CmdLineOpts::ListFPTargets;
static cl::opt<bool, true> ClListFPTargets("soaap-list-fp-targets",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("List function-pointer targets"),
       cl::location(CmdLineOpts::ListFPTargets));

bool CmdLineOpts::ListAllFuncs;
static cl::opt<bool, true> ClListAllFuncs("soaap-list-all-funcs",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("List all functions"),
       cl::location(CmdLineOpts::ListAllFuncs));

bool CmdLineOpts::SkipGlobalVariableAnalysis;
static cl::opt<bool, true> ClSkipGlobalVariableAnalysis("soaap-skip-global-variable-analysis",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Skip the analysis of global variable reads/writes"),
       cl::location(CmdLineOpts::SkipGlobalVariableAnalysis));

bool CmdLineOpts::Pedantic;
static cl::opt<bool, true> ClPedantic("soaap-pedantic",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Output all warnings"),
       cl::location(CmdLineOpts::Pedantic));

string CmdLineOpts::DebugModule;
static cl::opt<string, true> ClDebugModule("soaap-debug-module",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Output debug info for the specified SOAAP module pattern"),
       cl::location(CmdLineOpts::DebugModule));

string CmdLineOpts::DebugFunction;
static cl::opt<string, true> ClDebugFunction("soaap-debug-function",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Only output debug info for the specified SOAAP function pattern"),
       cl::location(CmdLineOpts::DebugFunction));

int CmdLineOpts::DebugVerbosity;
static cl::opt<int, true> ClDebugVerbosity("soaap-debug-verbosity",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Level of debug verbosity"),
       cl::location(CmdLineOpts::DebugVerbosity));

int CmdLineOpts::SummariseTraces;
static cl::opt<int, true> ClSummariseTraces("soaap-summarise-traces",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Summarise stack traces so that atmost the specified number of calls are shown from the top and the same number from the bottom of the trace"),
       cl::location(CmdLineOpts::SummariseTraces));

bool CmdLineOpts::DumpRPCGraph;
static cl::opt<bool, true> ClDumpRPCGraph("soaap-dump-rpc-graph",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Dump RPC Graph"),
       cl::location(CmdLineOpts::DumpRPCGraph));

OperatingSystemName CmdLineOpts::OperatingSystem;
static cl::opt<OperatingSystemName, true> ClOperatingSystem("soaap-os",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Operating system to model"),
       cl::values(
         clEnumValN(OperatingSystemName::FreeBSD, "freebsd", "FreeBSD (default)"),
         clEnumValN(OperatingSystemName::Linux, "linux", "Linux"),
       clEnumValEnd),
       cl::location(CmdLineOpts::OperatingSystem),
       cl::init(OperatingSystemName::FreeBSD)); // default value is Capsicum

SandboxPlatformName CmdLineOpts::SandboxPlatform;
static cl::opt<SandboxPlatformName, true> ClSandboxPlatform("soaap-sandbox-platform",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Sandbox platform to model"),
       cl::values(
         clEnumValN(SandboxPlatformName::None, "none", "None"),
         clEnumValN(SandboxPlatformName::Annotated, "annotated", "Annotated"),
         clEnumValN(SandboxPlatformName::Capsicum, "capsicum", "Capsicum (default)"),
         clEnumValN(SandboxPlatformName::Chroot, "chroot", "Chroot"),
         clEnumValN(SandboxPlatformName::Seccomp, "seccomp", "Secure Computing Mode (Seccomp)"),
         clEnumValN(SandboxPlatformName::SeccompBPF, "seccomp-bpf", "Secure Computing Mode BPF (Seccomp-BPF)"),
       clEnumValEnd),
       cl::location(CmdLineOpts::SandboxPlatform),
       cl::init(SandboxPlatformName::Capsicum)); // default value is Capsicum

string CmdLineOpts::SandboxPolicy;
static cl::opt<string, true> ClSandboxPolicyFile("soaap-sandbox-policy",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Sandbox-policy file"),
       cl::location(CmdLineOpts::SandboxPolicy));

bool CmdLineOpts::DumpDOTCallGraph;
static cl::opt<bool, true> ClDumpDOTCallGraph("soaap-dump-dot-callgraph",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Dump DOT CallGraph"),
       cl::location(CmdLineOpts::DumpDOTCallGraph));

bool CmdLineOpts::PrintCallGraph;
static cl::opt<bool, true> ClPrintCallGraph("soaap-print-callgraph",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Print CallGraph"),
       cl::location(CmdLineOpts::PrintCallGraph));

list<ReportOutputFormat> CmdLineOpts::ReportOutputFormats;
static cl::list<ReportOutputFormat, list<ReportOutputFormat> > ClReportOutputFormats("soaap-report-output-formats",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Comma-separated list of report-output formats"),
       cl::value_desc("list of report-output formats"),
       cl::values(
         clEnumValN(ReportOutputFormat::Text, "text", "Text (on stdout)"),
         clEnumValN(ReportOutputFormat::JSON, "json", "JSON"),
         clEnumValN(ReportOutputFormat::XML, "xml", "XML"),
         clEnumValN(ReportOutputFormat::HTML, "html", "HTML"),
       clEnumValEnd),
       cl::CommaSeparated,
       cl::location(CmdLineOpts::ReportOutputFormats));

string CmdLineOpts::ReportFilePrefix;
static cl::opt<string, true> ClReportFilePrefix("soaap-report-file-prefix",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Prefix for report-output filenames (default is "
                "\"soaap-output\")"),
       cl::location(CmdLineOpts::ReportFilePrefix),
       cl::init("soaap-output"));

SoaapMode CmdLineOpts::Mode;
static cl::opt<SoaapMode, true> ClMode("soaap-mode",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Mode to run SOAAP in"),
       cl::values(
         clEnumValN(SoaapMode::Null, "null", "Null"),
         clEnumValN(SoaapMode::Vuln, "vulnerable", "Vulnerability Analysis"),
         clEnumValN(SoaapMode::Correct, "correct", "Sandbox Correctness"),
         clEnumValN(SoaapMode::InfoFlow, "infoflow", "Information Flow Analysis"),
         clEnumValN(SoaapMode::Custom, "custom", "As per -soaap-analyses option (default)"),
         clEnumValN(SoaapMode::All, "all", "All"),
       clEnumValEnd),
       cl::location(CmdLineOpts::Mode),
       cl::init(SoaapMode::Custom)); // default value is Custom

list<SoaapAnalysis> CmdLineOpts::OutputTraces;
static cl::list<SoaapAnalysis, list<SoaapAnalysis> > ClOutputTraces("soaap-output-traces",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Comma-separated list of analyses for which to output traces"),
       cl::value_desc("list of analyses"),
       cl::values(
         clEnumValN(SoaapAnalysis::None, "none", "No Analysis"),
         clEnumValN(SoaapAnalysis::Vuln, "vulnerability", "Vulnerability Analysis (default)"),
         clEnumValN(SoaapAnalysis::Globals, "globals", "Global Variable Analysis"),
         clEnumValN(SoaapAnalysis::SysCalls, "syscalls", "System Call Analyses"),
         clEnumValN(SoaapAnalysis::PrivCalls, "privcalls", "Privileged Calls Analysis"),
         clEnumValN(SoaapAnalysis::SandboxedFuncs, "sandboxed", "Sandboxed Functions Analysis"),
         clEnumValN(SoaapAnalysis::InfoFlow, "infoflow", "Information Flow Analyses"),
         clEnumValN(SoaapAnalysis::All, "all", "All"),
       clEnumValEnd),
       cl::CommaSeparated,
       cl::location(CmdLineOpts::OutputTraces));

list<SoaapAnalysis> CmdLineOpts::SoaapAnalyses;
static cl::list<SoaapAnalysis, list<SoaapAnalysis> > ClSoaapAnalyses("soaap-analyses",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Comma-separated list of analyses to perform"),
       cl::value_desc("list of analyses"),
       cl::values(
         clEnumValN(SoaapAnalysis::None, "none", "No Analysis"),
         clEnumValN(SoaapAnalysis::Vuln, "vulnerability", "Vulnerability Analysis"),
         clEnumValN(SoaapAnalysis::Globals, "globals", "Global Variable Analysis"),
         clEnumValN(SoaapAnalysis::SysCalls, "syscalls", "System Call Analyses"),
         clEnumValN(SoaapAnalysis::PrivCalls, "privcalls", "Privileged Calls Analysis"),
         clEnumValN(SoaapAnalysis::SandboxedFuncs, "sandboxed", "Sandboxed Functions Analysis"),
         clEnumValN(SoaapAnalysis::InfoFlow, "infoflow", "Information Flow Analyses"),
         clEnumValN(SoaapAnalysis::All, "all", "All (default)"),
       clEnumValEnd),
       cl::CommaSeparated,
       cl::location(CmdLineOpts::SoaapAnalyses));

list<string> CmdLineOpts::WarnLibs;
static cl::list<string, list<string> > ClWarnLibs("soaap-warn-libs",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Comma-separated list of libraries SOAAP should only output warnings for"),
       cl::value_desc("list of libraries"),
       cl::CommaSeparated,
       cl::location(CmdLineOpts::WarnLibs));

list<string> CmdLineOpts::NoWarnLibs;
static cl::list<string, list<string> > ClNoWarnLibs("soaap-nowarn-libs",
       cl::cat(CmdLineOpts::SoaapCategory),
       cl::desc("Comma-separated list of libraries SOAAP should not output warnings for"),
       cl::value_desc("list of libraries"),
       cl::CommaSeparated,
       cl::location(CmdLineOpts::NoWarnLibs));
