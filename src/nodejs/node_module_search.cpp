#include "node_module_search.h"

#include <iostream>
#include <regex>

#include "file_ops.h"

bool node::is_core_module(string path) {
  std::string ext_path = path + ".js";

  if(is_file(ext_path.c_str()) == FILE_TYPE::PATH_TO_FILE) return true;

  return false;
}

string node::get_core_module(string path) {
  std::string ext_path = path + ".js";

  if(is_file(ext_path.c_str()) == FILE_TYPE::PATH_TO_FILE) {
    return ext_path;
  }

  return "";
}

void node::resolve_path(string path) {
  std::cout << "Resolve core module: " << path << std::endl;
}

char* node::load_as_file(const char* file) {
  string path = string(file);

  // checking if actual javascript file (accepts no .js end but nothing else)
  std::smatch m_1;
  std::regex e_1("(^(\\w|./|/)+((.js){1}|(?!(.\\w)))$)");
  std::regex_match(path,m_1,e_1);

  if(m_1.size() > 0) {
    int sourceLen;
    char* c_source = load_js_file(file, sourceLen);
    return c_source;
  }

  // checking json object only
  std::smatch m_2;
  std::regex e_2("(^(\\w|./|/)+((.json){1})$)");
  std::regex_match(path,m_2,e_2);

  if(m_1.size() > 0) {
    int sourceLen;
    string source = string(load_js_file(file, sourceLen));
    source = "exports="+source;

    // converting c++ string to char* source code for return
    sourceLen = source.length() + 1;
    char* c_source = new char [sourceLen];
    // updating the source code length
    strcpy(c_source, source.c_str());
    return c_source;
  }

  return 0;
}

void node::load_node_modules() {

}

void node::node_modules_paths() {

}
