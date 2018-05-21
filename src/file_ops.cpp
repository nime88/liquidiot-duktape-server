#include "file_ops.h"

#include <iostream>
#include <cstring>
#include <fstream>
#include <ios>
#include <string>

using namespace std;

char* load_js_file(const char* filename, int & sourceLen) {
  char* sourceCode;
  {
    ifstream ifs(filename);
    std::string content( (std::istreambuf_iterator<char>(ifs) ),
                     (std::istreambuf_iterator<char>()    ) );

    // converting c++ string to char* source code for return
    sourceCode = new char [content.length()+1];
    // updating the source code length
    sourceLen = content.length()+1;
    strcpy(sourceCode, content.c_str());
  }

  return sourceCode;
}
