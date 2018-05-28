#ifndef FILE_OPS_H
#define FILE_OPS_H

#include <map>
#include <string>

enum FILE_TYPE {
  PATH_TO_DIR,PATH_TO_FILE,OTHER,NOT_EXIST
};

extern char* load_js_file(const char* filename, int & sourceLen);

// Loads file as config
std::map<std::string, std::string> load_config(const char* filename);

FILE_TYPE is_file(const char* path);

#endif
