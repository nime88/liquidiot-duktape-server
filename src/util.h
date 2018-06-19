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

using namespace std;

extern map<string,string> read_package_json(duk_context *ctx, const char* package_js_src);
extern map<string,vector<string> > read_liquidiot_json(duk_context *ctx, const char* package_js_src);

#endif
