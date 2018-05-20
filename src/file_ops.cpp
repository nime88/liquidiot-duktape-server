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
    // opening the file
    fstream namefile;
    namefile.open(filename, ios::in);

    if(!namefile.good())
		  cout << "FILE OPENING ERROR!!" << endl;

    string org_source = "";

    // reading the file to org_source c++ string
    while (!namefile.eof()) {
      char temp_line[512];
      namefile.getline(temp_line, 512, '\n');
      org_source.append(temp_line);
      org_source.append("\n");
    }

    // converting c++ string to char* source code for return
    sourceCode = new char [org_source.length()+1];
    // updating the source code length
    sourceLen = org_source.length()+1;
    strcpy(sourceCode, org_source.c_str());
  }

  return sourceCode;
}
