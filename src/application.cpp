#include "application.h"

unsigned int JSApplication::interval_ = 0;
bool JSApplication::repeat_ = false;

JSApplication::JSApplication(const char* path){

  duk_context_ = duk_create_heap_default();
  if (!duk_context_) {
    throw "Duk context could not be created.";
  }

  // initializing print alerts
  duk_print_alert_init(duk_context_, 0 /*flags*/);

  // reding the source code (main.js) for the app
  int source_len;
  string file = string(path) + string("/main.js");
  source_code_ = load_js_file(file.c_str(),source_len);

  // creating the setTaskInterval function for the app to use
  set_task_interval_idx_ = duk_push_c_function(duk_context_, setTaskInterval, 2 /*nargs*/);
  duk_put_global_string(duk_context_, "setTaskInterval");
}

void JSApplication::run() {

  // evaluating the source code
  duk_push_string(duk_context_, source_code_);
  if (duk_peval(duk_context_) != 0) {
      /* Use duk_safe_to_string() to convert error into string.  This API
       * call is guaranteed not to throw an error during the coercion.
       */
      printf("Script error: %s\n", duk_safe_to_string(duk_context_, -1));
  }
  duk_pop(duk_context_);
  duk_get_global_string(duk_context_, "initialize");

  if(duk_pcall(duk_context_, 0) != 0) {
    printf("Script error: %s\n", duk_safe_to_string(duk_context_, -1));
  }

  if(!JSApplication::repeat_) {
    duk_get_global_string(duk_context_, "task");

    if(duk_pcall(duk_context_, 0) != 0) {
      printf("Script error: %s\n", duk_safe_to_string(duk_context_, -1));
    }
  } else {
    duk_get_global_string(duk_context_, "setInterval");
    duk_push_string(duk_context_, "task()");
    duk_push_int(duk_context_, JSApplication::interval_);
    if(duk_pcall(duk_context_, 2) != 0) {
      printf("Script error: %s\n", duk_safe_to_string(duk_context_, -1));
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
