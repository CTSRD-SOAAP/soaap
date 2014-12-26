#include "Util/ClassifiedUtils.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace soaap;
using namespace llvm;

int ClassifiedUtils::nextClassNameBitIdx = 0;
map<string,int> ClassifiedUtils::classNameToBitIdx;
map<int,string> ClassifiedUtils::bitIdxToClassName;

string ClassifiedUtils::stringifyClassNames(int classNames) {
  string classNamesStr = "[";
  int currIdx = 0;
  bool first = true;
  for (currIdx=0; currIdx<=31; currIdx++) {
    if (classNames & (1 << currIdx)) {
      string className = bitIdxToClassName[currIdx];
      if (!first) 
        classNamesStr += ",";
      classNamesStr += className;
      first = false;
    }
  }
  classNamesStr += "]";
  return classNamesStr;
}

StringVector ClassifiedUtils::convertNamesToVector(int classNames) {
  StringVector vec;
  int currIdx = 0;
  for (currIdx=0; currIdx<=31; currIdx++) {
    if (classNames & (1 << currIdx)) {
      string className = bitIdxToClassName[currIdx];
      vec.push_back(className);
    }
  }
  return vec;
}

void ClassifiedUtils::assignBitIdxToClassName(string className) {
  if (classNameToBitIdx.find(className) == classNameToBitIdx.end()) {
    dbgs() << "    Assigning index " << nextClassNameBitIdx << " to class name \"" << className << "\"\n";
    classNameToBitIdx[className] = nextClassNameBitIdx;
    bitIdxToClassName[nextClassNameBitIdx] = className;
    nextClassNameBitIdx++;
  }
}

int ClassifiedUtils::getBitIdxFromClassName(string className) {
  return classNameToBitIdx[className];
}
