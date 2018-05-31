#include "application.h"

#include <algorithm>
#include "node_module_search.h"

unsigned int JSApplication::interval_ = 0;
bool JSApplication::repeat_ = false;
vector<string> JSApplication::app_paths_;
map<string,string> JSApplication::options_ = load_config("config.cfg");

JSApplication::JSApplication(const char* path){

  duk_context_ = duk_create_heap_default();
  if (!duk_context_) {
    throw "Duk context could not be created.";
  }

  // initializing print alerts
  duk_print_alert_init(duk_context_, 0 /*flags*/);

  // console
  duk_console_init(duk_context_, DUK_CONSOLE_PROXY_WRAPPER /*flags*/);

  // initializing node.js require
  duk_push_object(duk_context_);
  duk_push_c_function(duk_context_, cb_resolve_module, DUK_VARARGS);
  duk_put_prop_string(duk_context_, -2, "resolve");
  duk_push_c_function(duk_context_, cb_load_module, DUK_VARARGS);
  duk_put_prop_string(duk_context_, -2, "load");
  duk_module_node_init(duk_context_);

  // registering the eventloop
  eventloop_register(duk_context_);

  // loading the event loop javascript functions (setTimeout ect.)
  int evlLen;
  char* evlSource = load_js_file("./c_eventloop.js",evlLen);
  duk_push_string(duk_context_, evlSource);
  if (duk_peval(duk_context_) != 0) {
    printf("Script error 1: %s\n", duk_safe_to_string(duk_context_, -1));
  } else {
    printf("result is: %s\n", duk_safe_to_string(duk_context_, -1));
  }
  duk_pop(duk_context_);

  // setting some globals
  // duk_push_string(duk_context_, "arguments = [];\n");
  // if (duk_peval(duk_context_) != 0) {
  //   printf("Script error 2: %s\n", duk_safe_to_string(duk_context_, -1));
  // } else {
  //   printf("result is: %s\n", duk_safe_to_string(duk_context_, -1));
  // }

  // reding the source code (main.js) for the app
  int source_len;
  // adding app_path to global app paths
  app_paths_.push_back(string(path));
  string file = string(path) + string("/main.js");

  // source_code_ = load_js_file(file.c_str(),source_len);

  int source_len2;
  string temp_source = string(load_js_file("./application_header.js",source_len2)) + string(load_js_file(file.c_str(),source_len)) + "\ninitialize();";
  // executing initialize code
  source_code_ = new char[temp_source.length()+1];
  strcpy(source_code_,temp_source.c_str());

  // pushing this as main module
  duk_push_string(duk_context_, source_code_);
  duk_module_node_peval_main(duk_context_, file.c_str());

}

void JSApplication::run() {

  int rc = duk_safe_call(duk_context_, eventloop_run, NULL, 0 /*nargs*/, 1 /*nrets*/);
  if (rc != 0) {
    fprintf(stderr, "eventloop_run() failed: %s\n", duk_to_string(duk_context_, -1));
    fflush(stderr);
  }
  duk_pop(duk_context_);

}

duk_ret_t JSApplication::cb_resolve_module(duk_context *ctx) {
  /*
   *  Entry stack: [ requested_id parent_id ]
   */

  const char *requested_id;
  const char *parent_id;

  // TODO if require string fails we have no way of knowing
  requested_id = duk_require_string(ctx, 0);
  parent_id = duk_require_string(ctx, 1);

  string requested_id_str = string(requested_id);
  // getting the known application/module path
  string path = app_paths_.at(app_paths_.size()-1) + "/";

  // if buffer is requested just use native duktape implemented buffer
  if(requested_id_str == "buffer") {
    duk_push_string(ctx, requested_id);
    return 1;  /*nrets*/
  }

  if(requested_id_str.size() > 0) {
    // First checking core modules
    if (requested_id_str.at(0) != '.' && requested_id_str.at(0) != '/' &&
        options_.find("core_lib_path") != options_.end()) {
      string core_lib_path = options_.at("core_lib_path");
      if(core_lib_path.size() > 0) {
        if (node::is_core_module(core_lib_path + "/" + string(requested_id))) {
          cout << "Core Module found" << endl;
          requested_id_str = node::get_core_module(core_lib_path + "/" + string(requested_id));
          duk_push_string(ctx, requested_id_str.c_str());
          return 1;
        } else if(is_file((core_lib_path + "/" + requested_id).c_str()) == FILE_TYPE::PATH_TO_DIR) {
          requested_id_str = (core_lib_path + "/" + requested_id + "/");
          duk_push_string(ctx, requested_id_str.c_str());
          return 1;
        }
      }
    }

    // Then checking if path is system root
    if(requested_id_str.at(0) == '/') {
      path = "";
    }

    if(requested_id_str.substr(0,2) == "./" || requested_id_str.at(0) ==  '/' || requested_id_str.substr(0,3) == "../") {
      if(is_file((path + requested_id).c_str()) == FILE_TYPE::PATH_TO_FILE) {
        requested_id_str = path + requested_id;
      }
      else if(is_file((path + requested_id).c_str()) == FILE_TYPE::PATH_TO_DIR) {
        requested_id_str = path + requested_id;
      }
    }
  }

  cout << "Req id: " << requested_id << endl;
  cout << "Parent id: " << parent_id << endl;
  cout << "Resolved id 1: " << requested_id_str << endl;

  duk_push_string(ctx, requested_id_str.c_str());
  return 1;  /*nrets*/
}

duk_ret_t JSApplication::cb_load_module(duk_context *ctx) {
    /*
     *  Entry stack: [ resolved_id exports module ]
     */

    const char *resolved_id = duk_require_string(ctx, 0);
    duk_get_prop_string(ctx, 2, "filename");
    const char *filename = duk_require_string(ctx, -1);

    if (string(resolved_id) == "buffer") {
      duk_push_string(ctx,"module.exports.Buffer = Buffer;");
      return 1;
    }

    printf("load_cb: id:'%s', filename:'%s'\n", resolved_id, filename);

    char *module_source = 0;

    // if resolved id is an actual file we try to load it as javascript
    if(is_file(resolved_id) == FILE_TYPE::PATH_TO_FILE) {
      module_source = node::load_as_file(resolved_id);
      if(!module_source) {
        (void) duk_type_error(ctx, "cannot find module: %s", resolved_id);
        return 1;
      }
    }
    // if resolved to directory we try to load it as such
    else if(is_file(resolved_id) == FILE_TYPE::PATH_TO_DIR) {
      // first we have to read the package.json
      string path = string(resolved_id);
      path = path + "package.json";

      if(is_file(path.c_str()) == FILE_TYPE::PATH_TO_FILE) {
        int sourceLen;
        char* package_js = load_js_file(path.c_str(),sourceLen);

        // reading the main field from package.json
        duk_push_string(ctx, package_js);
        duk_json_decode(ctx, -1);
        duk_get_prop_string(ctx, -1, "main");
        const char* main_js = duk_to_string(ctx, -1);
        duk_pop_2(ctx);

        // loading main file to source
        path = string(resolved_id) + string(main_js);
        module_source = node::load_as_file(path.c_str());
        if(!module_source) {
          (void) duk_type_error(ctx, "cannot find module: %s", resolved_id);
          return 1;
        }
      }
    }

    if(!module_source) {
      (void) duk_type_error(ctx, "cannot find module: %s", resolved_id);
      return 1;
    }

    string source = module_source;

    if(source.size() > 0) {
      // pushing the source code to heap as the require system wants that
      duk_push_string(ctx, module_source);
      // if (duk_peval(ctx) != 0) {
      //     printf("Script error 134: %s\n", duk_safe_to_string(ctx, -1));
      // }
    } else {
      (void) duk_type_error(ctx, "cannot find module: %s", resolved_id);
    }

    return 1;  /*nrets*/
}
