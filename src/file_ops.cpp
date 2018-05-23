#include "file_ops.h"

#include <iostream>
#include <cstring>
#include <fstream>
#include <ios>
#include <string>
#include <sstream>

using namespace std;

char* load_js_file(const char* filename, int & sourceLen) {
  char* sourceCode;

  cout << "Filename: " << filename << endl;

  ifstream file(filename);
  std::string content( (std::istreambuf_iterator<char>(file) ),
                     (std::istreambuf_iterator<char>()    ) );

  // converting c++ string to char* source code for return
  sourceLen = content.length() + 1;
  sourceCode = new char [sourceLen];
  // updating the source code length
  strcpy(sourceCode, content.c_str());

  if (file.is_open()) file.close();

  if(sourceLen <= 1) {
    cout << "File " << filename << " doesn't exist or is empty." << endl;
  }

  return sourceCode;
}
