#include "node_module_search.h"

#include <iostream>

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

void node::load_as_file() {

}

void node::load_index() {

}

void node::load_as_directory() {

}

void node::load_node_modules() {

}

void node::node_modules_paths() {

}
