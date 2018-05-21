#include "duk_manager.h"
#include "file_ops.h"

int DukManager::executeFile(const char* filename){
  int sourceLen;
  char* sourceCode = load_js_file(filename, sourceLen);

  if(!sourceCode)
    return 0;

  duk_push_string(context, sourceCode);
  duk_eval(context);
  duk_pop(context);

  return 1;
}

int DukManager::executeSource(const char* source) {
  if(!source)
    return 0;

  duk_push_string(context, source);
  duk_eval(context);
  duk_pop(context);

  return 1;
}
