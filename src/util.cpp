#include "util.h"

#include <iostream>

map<string,string> read_json_file(duk_context *ctx, const char* package_js_src) {
  map<string,string> attr;
  // reading the main field from package.json
  duk_push_string(ctx, package_js_src);
  duk_json_decode(ctx, -1);
  duk_get_prop_string(ctx, -1, "main");
  const char* main_js = duk_to_string(ctx, -1);
  duk_pop_2(ctx);
  attr.insert(pair<string,string>("main",string(main_js)));

  return attr;
}
