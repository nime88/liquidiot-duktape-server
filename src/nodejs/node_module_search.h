/*
 * This is some implementation for the nodejs module search described in
 * https://nodejs.org/api/modules.html
 */
#if !defined(NODE_MODULE_SEARCH_H_INCLUDED)
#define NODE_MODULE_SEARCH__H_INCLUDED

namespace node {
  void resolve_path();

  void load_as_file();

  void load_index();

  void load_as_directory();

  void load_node_modules();

  void node_modules_paths();
}

#endif
