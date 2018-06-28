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
extern map<string, map<string,string> >get_config(duk_context *ctx);
extern void save_config(duk_context *ctx, map<string, map<string,string> > new_config);

extern map<string,string> read_package_json(duk_context *ctx, const char* package_js_src);
extern map<string,vector<string> > read_liquidiot_json(duk_context *ctx, const char* package_js_src);
extern map<string,string> read_raw_json(duk_context *ctx, const char* js_src, map<string,string> &attr);

#endif
