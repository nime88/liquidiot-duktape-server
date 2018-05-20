/*
 *  Very simple example program
 */

#if defined (__cplusplus)
extern "C" {
#endif

  #include <stdio.h>
  #include "duktape.h"
  #include "duk_print_alert.h"

#if defined (__cplusplus)
}
#endif

#include "file_ops.h"

// static duk_ret_t native_print(duk_context *ctx) {
//         duk_push_string(ctx, " ");
//         duk_insert(ctx, 0);
//         duk_join(ctx, duk_get_top(ctx) - 1);
//         printf("%s\n", duk_safe_to_string(ctx, -1));
//         return 0;
// }
//
// static duk_ret_t native_adder(duk_context *ctx) {
//         int i;
//         int n = duk_get_top(ctx);  /* #args */
//         double res = 0.0;
//
//         for (i = 0; i < n; i++) {
//                 res += duk_to_number(ctx, i);
//         }
//
//         duk_push_number(ctx, res);
//         return 1;  /* one return value */
// }

int main(int argc, char *argv[]) {
  duk_context *ctx;

  ctx = duk_create_heap_default();
  if (!ctx) {
    return 1;
  }

  // initializing prints to help debugging (probably will not bee needed when all is done)
  duk_print_alert_init(ctx, 0 /*flags*/);

  (void) argc; (void) argv;  /* suppress warning */

  int sourceLen;
  char* sourceCode = load_js_file("main.js", sourceLen);

  duk_push_string(ctx, sourceCode);
  duk_eval(ctx);
  duk_pop(ctx);


  duk_destroy_heap(ctx);

  return 0;
}
