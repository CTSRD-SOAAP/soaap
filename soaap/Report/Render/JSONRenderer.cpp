#include "Report/Render/JSONRenderer.h"

#include "Common/CmdLineOpts.h"
#include "Report/IR/Report.h"

#include "llvm/Support/raw_ostream.h"

#include <iostream>
#include <fstream>
#include <vector>

using namespace llvm;
using namespace soaap;
using namespace std;

JSONRenderer::JSONRenderer() : Renderer(false) {
  writer = new PrettyWriter<StringBuffer>(s);
}

void JSONRenderer::visit(Report* r) {
  writer->StartObject();
  vector<Vulnerability*> vulns = r->getVulnerabilities();
  writer->Key("vulnerabilities");
  writer->StartArray();
  for (Vulnerability* v : vulns) {
    visit(v);
  }
  writer->EndArray();
  writer->EndObject();
  ofstream file;
  string filename; 
  if (CmdLineOpts::ReportFilePrefix.empty()) {
    filename = "output";
  }
  else {
    filename = CmdLineOpts::ReportFilePrefix;
  }
  filename += ".json";
  file.open(filename);
  file << s.GetString();
  file.close();
}

void JSONRenderer::visit(Vulnerability* v) {
  writer->StartObject();
  writer->Key("location");
  SourceLocation loc = v->getLocation();
  visit(&loc);

  writer->Key("vuln_vendor");
  writer->Bool(v->isVulnVendorLoc());

  if (!v->isVulnVendorLoc()) {
    writer->Key("cves");
    writer->StartArray();
    for (string cve : v->getCVEs()) {
      writer->String(cve.c_str());
    }
    writer->EndArray();
  }

  writer->Key("sandboxed");
  writer->Bool(v->isSandboxed());

  if (v->isSandboxed()) {
    writer->Key("sandbox");
    writer->String(v->getSandbox()->getName().c_str());
  }

  writer->Key("call_stack");
  writer->StartArray();
  for (SourceLocation s : v->getStack()) {
    visit(&s);
  }
  writer->EndArray();

  writer->Key("leaks_limited_right");
  writer->Bool(v->leaksLimitedRights());

  if (v->leaksLimitedRights()) {
    writer->Key("rights");
    writer->StartObject();
    
    writer->Key("globals");
    writer->StartArray();
    for (pair<GlobalVariable*, vector<VariableRight> > p : v->getGlobals()) {
      writer->StartObject();
      GlobalVariable* G = p.first;
      writer->Key("var_name");
      writer->String(G->getName().str().c_str());
      writer->Key("rights");
      writer->StartArray();
      for (VariableRight r : p.second) {
        switch (r) {
          case READ: {
            writer->String("Read");
            break;
          }
          case WRITE: {
            writer->String("Write");
            break;
          }
          default: { }
        }
      }
      writer->EndArray();
      writer->EndObject();
    }
    writer->EndArray();
    
    writer->Key("privates");
    writer->StartArray();
    for (pair<Value*,VariableType> p : v->getPrivates()) {
      writer->StartObject();
      writer->Key("var_name");
      writer->String(p.first->getName().str().c_str());
      writer->Key("var_type");
      switch (p.second) {
        case LOCAL: {
          writer->String("local");
          break;
        }
        case GLOBAL: {
          writer->String("global");
          break;
        }
        case STRUCT_MEMBER: {
          writer->String("struct_member");
          break;
        }
        default: { }
      }
      writer->EndObject();
    }
    writer->EndArray();
     
    writer->Key("callgates");
    writer->StartArray();
    for (Function* F : v->getCallgates()) {
      writer->String(F->getName().str().c_str());
    }
    writer->EndArray();
    
    writer->Key("capabilities");
    writer->StartArray();
    for (pair<const Value*,FunctionSet> p : v->getCapabilities()) {
      writer->StartObject();
      writer->Key("var_name");
      const Value* v = p.first;
      writer->String(v->getName().str().c_str());
      writer->Key("syscalls");
      writer->StartArray();
      for (Function* F : p.second) {
        writer->String(F->getName().str().c_str());
      }
      writer->EndArray();
      writer->EndObject();
    }
    writer->EndArray();

    writer->EndObject();
  }
  writer->EndObject();
}

void JSONRenderer::visit(SourceLocation* s) {
  writer->StartObject();
  writer->Key("function");
  writer->String(s->getFunction()->getName().str().c_str());
  writer->Key("filename");
  writer->String(s->getFilename().c_str());
  writer->Key("line_number");
  writer->Int(s->getLineNumber());
  writer->EndObject();
}
