#include "application.h"

unsigned int JSApplication::interval_ = 0;
bool JSApplication::repeat_ = false;
vector<string> JSApplication::app_paths_;

JSApplication::JSApplication(const char* path){

  duk_context_ = duk_create_heap_default();
  if (!duk_context_) {
    throw "Duk context could not be created.";
  }

  // initializing print alerts
  duk_print_alert_init(duk_context_, 0 /*flags*/);

  // initializing node.js require
  duk_push_object(duk_context_);
  duk_push_c_function(duk_context_, cb_resolve_module, DUK_VARARGS);
  duk_put_prop_string(duk_context_, -2, "resolve");
  duk_push_c_function(duk_context_, cb_load_module, DUK_VARARGS);
  duk_put_prop_string(duk_context_, -2, "load");
  duk_module_node_init(duk_context_);

  // reding the source code (main.js) for the app
  int source_len;
  // adding app_path to global app paths
  app_paths_.push_back(string(path));
  string file = string(path) + string("/main.js");

  duk_module_node_peval_main(duk_context_, file.c_str());

  source_code_ = load_js_file(file.c_str(),source_len);

  // creating the setTaskInterval function for the app to use
  set_task_interval_idx_ = duk_push_c_function(duk_context_, setTaskInterval, 2 /*nargs*/);
  duk_put_global_string(duk_context_, "setTaskInterval");


}

void JSApplication::run() {

  // evaluating the source code
  duk_push_string(duk_context_, source_code_);
  if (duk_peval(duk_context_) != 0) {
      printf("Script error 45: %s\n", duk_safe_to_string(duk_context_, -1));
  }

  duk_pop(duk_context_);
  duk_get_global_string(duk_context_, "initialize");

  if(duk_pcall(duk_context_, 0) != 0) {
    printf("Script error 52: %s\n", duk_safe_to_string(duk_context_, -1));
  }

  if(!JSApplication::repeat_) {
    duk_get_global_string(duk_context_, "task");

    if(duk_pcall(duk_context_, 0) != 0) {
      printf("Script error 59: %s\n", duk_safe_to_string(duk_context_, -1));
    }
  } else {
    duk_get_global_string(duk_context_, "setInterval");
    duk_push_string(duk_context_, "task()");
    duk_push_int(duk_context_, JSApplication::interval_);
    if(duk_pcall(duk_context_, 2) != 0) {
      printf("Script error 66: %s\n", duk_safe_to_string(duk_context_, -1));
    }
  }
}

duk_ret_t JSApplication::setTaskInterval(duk_context* ctx) {
  unsigned int interval = duk_get_int(ctx,-1);
  duk_pop(ctx);
  unsigned int repeat = duk_get_boolean(ctx,-1);
  duk_pop(ctx);

  JSApplication::interval_ = interval;
  JSApplication::repeat_ = repeat;

  // TODO Clean up later
  cout << "Task Interval set to " << repeat << " and " << interval << endl;

  return 0;
}

duk_ret_t JSApplication::cb_resolve_module(duk_context *ctx) {
  const char *requested_id;
  const char *parent_id;
  const char *app_path;

  requested_id = duk_require_string(ctx, 0);
  parent_id = duk_require_string(ctx, 1);

  // getting the known application/module path
  app_path = app_paths_.at(app_paths_.size()-1).c_str();

  string requested_id_str = string(app_path) + "/%s";

  // duk_push_sprintf(ctx, requested_id_str.c_str(), requested_id);

  cout << "App path: " << app_path << endl;

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

    duk_get_prop_string(ctx, 2, "filename");
    filename = duk_require_string(ctx, -1);

    printf("load_cb: id:'%s', filename:'%s'\n", resolved_id, filename);

    // TODO atm there are no exports or modules at this phase
    // const char *exports = duk_get_string(ctx, 1);
    // const char *module = duk_get_string(ctx, 2);

    int sourceLen;
    char *module_source = load_js_file(resolved_id, sourceLen);

    duk_push_string(ctx, module_source);
    if (duk_peval(ctx) != 0) {
        printf("Script error 134: %s\n", duk_safe_to_string(ctx, -1));
    }

    return 1;  /*nrets*/
}
