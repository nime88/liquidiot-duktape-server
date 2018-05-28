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

  // setting default assert handling
  duk_push_c_function(duk_context_, handle_assert, 2);
  duk_put_global_string(duk_context_, "assert");

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

  // creating the setInterval function for the app to use
  // set_task_interval_idx_ = duk_push_c_function(duk_context_, setInterval, 2 /*nargs*/);
  // duk_put_global_string(duk_context_, "setInterval");

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

  // // evaluating the source code
  // duk_push_string(duk_context_, source_code_);
  // if (duk_peval(duk_context_) != 0) {
  //     printf("Script error 45: %s\n", duk_safe_to_string(duk_context_, -1));
  // }
  //
  // duk_pop(duk_context_);
  // duk_get_global_string(duk_context_, "initialize");

  // duk_push_string(duk_context_, "exports.initialize();");
  // if(duk_peval(duk_context_) != 0) {
  //   printf("Script error 52: %s\n", duk_safe_to_string(duk_context_, -1));
  // }

  // if(!JSApplication::repeat_) {
  //   duk_get_global_string(duk_context_, "task");
  //
  //   if(duk_pcall(duk_context_, 0) != 0) {
  //     printf("Script error 59: %s\n", duk_safe_to_string(duk_context_, -1));
  //   }
  // }
  // else {
  //   duk_get_global_string(duk_context_, "setInterval");
  //   duk_push_string(duk_context_, "task()");
  //   duk_push_int(duk_context_, JSApplication::interval_);
  //   if(duk_pcall(duk_context_, 2) != 0) {
  //     printf("Script error 66: %s\n", duk_safe_to_string(duk_context_, -1));
  //   }
  // }
}

duk_ret_t JSApplication::setInterval(duk_context* ctx) {
  duk_require_function(ctx,-1);
  if (duk_pcall(ctx,-1) != 0) {
    printf("Script error 1: %s\n", duk_safe_to_string(ctx, -1));
  }

  duk_pop(ctx);
  unsigned int interval = duk_get_boolean(ctx,-1);
  duk_pop(ctx);

  JSApplication::interval_ = interval;
  // JSApplication::repeat_ = repeat;

  // TODO Clean up later
  cout << "Task Interval set to " << interval << endl;

  // if (duk_peval_string(ctx, "var exports = require('main.js');print(JSON.stringify(exports));exports.task();") != 0) {
  //   printf("eval failed: %s\n", duk_safe_to_string(ctx, -1));
  // } else {
  //   printf("result is: %s\n", duk_get_string(ctx, -1));
  // }
  // duk_pop(ctx);

  return 0;
}

duk_ret_t JSApplication::cb_resolve_module(duk_context *ctx) {
  /*
   *  Entry stack: [ requested_id parent_id ]
   */

  const char *requested_id;
  const char *parent_id;
  const char *app_path;

  // TODO if require string fails we have no way of knowing
  requested_id = duk_require_string(ctx, 0);
  parent_id = duk_require_string(ctx, 1);

  string requested_id_str = "";

  // if buffer is requested just use native implemented buffer
  if(string(requested_id) == "buffer") {
    duk_push_string(ctx, requested_id);
    return 1;  /*nrets*/
  } else if(string(requested_id) == "process") {
    duk_push_string(ctx, requested_id);
    return 1;  /*nrets*/
  }

  // First checking nodes core modules
  if (options_.find("node_core_lib_path") != options_.end()) {
    string node_core_lib_path = options_.at("node_core_lib_path");
    if(node_core_lib_path.size() > 0) {
      if (node::is_core_module(node_core_lib_path + "/" + string(requested_id))) {
        cout << "Core Module found" << endl;
        requested_id_str = node::get_core_module(node_core_lib_path + "/" + string(requested_id));
      }
    }
  }

  if(requested_id_str.size() > 0) {
    duk_push_string(ctx, requested_id_str.c_str());
    return 1;
  }

  // getting the known application/module path
  app_path = app_paths_.at(app_paths_.size()-1).c_str();

  requested_id_str = string(app_path) + "/%s";

  // duk_push_sprintf(ctx, requested_id_str.c_str(), requested_id);

  cout << "Req id: " << requested_id << endl;
  cout << "Parent id: " << parent_id << endl;

  string temp_req_id = string(requested_id);

  cout << "Substr:" <<  temp_req_id.substr(temp_req_id.size()-3,temp_req_id.size()-1) << endl;
  if(temp_req_id.size() > 3 && temp_req_id.substr(temp_req_id.size()-3,temp_req_id.size()-1) == ".js") {
    cout << "Comb str: " << string(app_path) + "/" + temp_req_id << endl;
    requested_id_str = string(app_path) + "/" + temp_req_id;
  } else {
  }

  cout << "Resolved id 1: " << requested_id_str << endl;

  duk_push_string(ctx, requested_id_str.c_str());
  return 1;  /*nrets*/
}

duk_ret_t JSApplication::cb_load_module(duk_context *ctx) {
    /*
     *  Entry stack: [ resolved_id exports module ]
     */
    const char *filename;

    const char *resolved_id = duk_require_string(ctx, 0);

    string resolved_id_str = "";

    if (string(resolved_id) == "buffer") {
      duk_push_string(ctx,"module.exports.Buffer = Buffer");
      return 1;
    } else if(string(resolved_id) == "process") {
      if (options_.find("node_core_lib_path") != options_.end()) {
        string node_core_lib_path = options_.at("node_core_lib_path");
        if(node_core_lib_path.size() > 0) {
          if (node::is_core_module(node_core_lib_path + "/internal/" + string(resolved_id))) {
            cout << "Core Module found" << endl;
            resolved_id_str = node::get_core_module(node_core_lib_path + "/internal/" + string(resolved_id));
          }
        }
      }
    }

    // const char *exports = duk_get_prop(ctx, 1);

    if( duk_is_object(ctx, 1) /* exports != NULL */) {
      cout << "Exports: " /* << exports */ << endl;
    } else {
      printf("Script error no exports: %s\n", duk_safe_to_string(ctx, 1));
    }

    duk_get_prop_string(ctx, 2, "filename");
    filename = duk_require_string(ctx, -1);

    printf("load_cb: id:'%s', filename:'%s'\n", resolved_id, filename);

    // TODO atm there are no exports or modules at this phase for some reason
    // const char *exports = duk_get_string(ctx, 1);
    // const char *module = duk_get_string(ctx, 2);

    // cout << "Module: " << module << endl;
    int sourceLen;
    char *module_source;
    if(resolved_id_str.size() >  0) {
      module_source = load_js_file(resolved_id_str.c_str(), sourceLen);
    } else {
      module_source = load_js_file(resolved_id, sourceLen);
    }

    string source = module_source;
    // replace(source.begin(),source.end(),'`','\'');

    duk_push_string(ctx, source.c_str());
    // if (duk_peval(ctx) != 0) {
    //     printf("Script error 134: %s\n", duk_safe_to_string(ctx, -1));
    // }

    return 1;  /*nrets*/
}

duk_ret_t JSApplication::handle_assert(duk_context *ctx) {
  cout << "MUAA" << endl;
	if (duk_to_boolean(ctx, 0)) {
		return 0;
	}
	(void) duk_generic_error(ctx, "assertion failed: %s", duk_safe_to_string(ctx, 1));
	return 0;
}
