#ifndef SOAAP_REPORT_RENDER_JSONRENDERER_H
#define SOAAP_REPORT_RENDER_JSONRENDERER_H

#include "Report/Render/Renderer.h"

#include <algorithm>
#include <vector>

#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

using namespace std;
using namespace rapidjson;

// see JSONSchema.json for the schema
namespace soaap {
  class JSONRenderer : public Renderer {
    public:
      JSONRenderer();
      virtual void visit(Report* r);
      virtual void visit(Vulnerability* v);
      virtual void visit(SourceLocation* s);

    protected:
      StringBuffer s;
      PrettyWriter<StringBuffer>* writer;
  };
}
#endif
