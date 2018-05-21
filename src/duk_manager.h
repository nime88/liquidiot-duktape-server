#if !defined(DUK_MANAGER_H_INCLUDED)
#define DUK_MANAGER__H_INCLUDED

#if defined (__cplusplus)
extern "C" {
#endif

  #include <stdio.h>
  #include "duktape.h"
  #include "duk_print_alert.h"

#if defined (__cplusplus)
}
#endif

class DukManager {
  public:
    DukManager() {
      context = duk_create_heap_default();
      if (!context) {
        throw "Duk context could not be created.";
      }

      // initializing prints to help debugging (probably will not bee needed when all is done)
      duk_print_alert_init(context, 0 /*flags*/);
    }

    ~DukManager() {
      duk_destroy_heap(context);
      context = 0;
    }

    duk_context* getContext() {
      return context;
    }

    // executes the contents of the javascript file
    int executeFile(const char* filename);

    int executeSource(const char* source);

  private:
    duk_context *context;
};

#endif
