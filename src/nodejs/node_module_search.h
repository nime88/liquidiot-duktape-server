/*
 * This is some implementation for the nodejs module search described in
 * https://nodejs.org/api/modules.html
 */
#ifndef NODE_MODULE_SEARCH_H_INCLUDED
#define NODE_MODULE_SEARCH_H_INCLUDED

#include <string>
using std::string;

namespace node {
  bool is_core_module(string path);

  string get_core_module(string path);

  void resolve_path(string path);

  void load_as_file();

  void load_index();

  void load_as_directory();

  void load_node_modules();

  void node_modules_paths();
}

#endif
