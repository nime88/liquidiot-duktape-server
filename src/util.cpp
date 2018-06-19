#include "util.h"

#include <iostream>
using namespace std;

map<string,string> read_package_json(duk_context *ctx, const char* js_src) {
  map<string,string> attr;

  // decoding source
  duk_push_string(ctx, js_src);
  duk_json_decode(ctx, -1);

  if(duk_has_prop_string(ctx, -1, "main")) {
    duk_get_prop_string(ctx, -1, "main");
    const char* main_js = duk_to_string(ctx, -1);
    duk_pop(ctx);
    attr.insert(pair<string,string>("main",string(main_js)));
  }

  duk_pop(ctx);

  return attr;
}

map<string,vector<string> > read_liquidiot_json(duk_context *ctx, const char* js_src) {
  map<string,vector<string> > attr;
  vector<string> dc_vec;
  vector<string> ai_vec;

  // decoding source
  duk_push_string(ctx, js_src);
  duk_json_decode(ctx, -1);

  if(duk_has_prop_string(ctx, -1, "deviceCapabilities")) {
    duk_get_prop_string(ctx, -1, "deviceCapabilities");

    duk_idx_t index = 0;
    while(duk_get_prop_index(ctx,-1,index)) {
      dc_vec.push_back(duk_to_string(ctx,-1));
      duk_pop(ctx);
      index++;
    }

    duk_pop(ctx);

    attr.insert(pair<string,vector<string> >("deviceCapabilities", dc_vec));
  }

  duk_pop(ctx);

  if(duk_has_prop_string(ctx, -1, "applicationInterfaces")) {
    duk_get_prop_string(ctx, -1, "applicationInterfaces");

    duk_idx_t index = 0;
    while(duk_get_prop_index(ctx,-1,index)) {
      ai_vec.push_back(duk_to_string(ctx,-1));
      duk_pop(ctx);
      index++;
    }

    duk_pop(ctx);

    attr.insert(pair<string,vector<string> >("applicationInterfaces", ai_vec));
  }

  duk_pop_2(ctx);

  return attr;
}
