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
#include <string>

using namespace std;

extern map<string,string> read_json_file(duk_context *ctx, const char* package_js_src);

#endif
