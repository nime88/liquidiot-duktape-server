#if !defined(UTIL_H_INCLUDED)
#define UTIL_H_INCLUDED

#if defined (__cplusplus)
extern "C" {
#endif

  #include "duktape.h"

#if defined (__cplusplus)
}
#endif

#include <map>
#include <vector>
#include <string>

using std::map;
using std::string;
using std::vector;

// gets the full config data structu
// duk_context is needed for parsing
extern duk_int_t safe_json_decode(duk_context *ctx, const char* json);
extern map<string, map<string,string> >get_config(duk_context *ctx, const string& exec_path);
extern void save_config(duk_context *ctx, map<string, map<string,string> > new_config, const string& exec_path);

extern map<string,string> read_package_json(duk_context *ctx, const char* package_js_src);
extern map<string,vector<string> > read_liquidiot_json(duk_context *ctx, const char* package_js_src);

#endif
