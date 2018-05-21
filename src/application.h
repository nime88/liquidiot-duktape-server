#if !defined(APPLICATION_H_INCLUDED)
#define APPLICATION__H_INCLUDED

#include "file_ops.h"

#include <string>
#include <iostream>

using namespace std;

class JSApplication {
  public:
    JSApplication(const char* path) {
      int source_len;
      string file = string(path) + string("/main.js");
      source_code_ = load_js_file(file.c_str(),source_len);
      cout << source_code_;
    }

    char* getJSSource() {
      return source_code_;
    }

  private:
    string name_;
    char* source_code_;
};

#endif
