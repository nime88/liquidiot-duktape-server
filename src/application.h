#if !defined(APPLICATION_H_INCLUDED)
#define APPLICATION__H_INCLUDED

#include "file_ops.h"

#if defined (__cplusplus)
extern "C" {
#endif

  #include <stdio.h>
  #include "duktape.h"
  #include "duk_print_alert.h"
  #include "duk_module_node.h"

#if defined (__cplusplus)
}
#endif

#include <iostream>
#include <string>
#include <vector>
using namespace std;

class JSApplication {
  public:
    JSApplication(const char* path);

    ~JSApplication() {
      duk_get_global_string(duk_context_, "terminate");

      if(duk_pcall(duk_context_, 0) != 0) {
        printf("Script error: %s\n", duk_safe_to_string(duk_context_, -1));
      }
      duk_destroy_heap(duk_context_);
      duk_context_ = 0;
    }

    char* getJSSource() {
      return source_code_;
    }

    void run();

    static duk_ret_t setTaskInterval(duk_context *ctx);
    static duk_ret_t cb_resolve_module(duk_context *ctx);
    static duk_ret_t cb_load_module(duk_context *ctx);

  private:
    string name_;
    duk_context *duk_context_;
    char* source_code_;
    static unsigned int interval_;
    static bool repeat_;
    static vector<string> app_paths_;

    duk_idx_t set_task_interval_idx_;
};

#endif
