#include "duktape_interface.h"

#include "duk_util.h"
#include "duktape_callbacks.h"
#include "mod_duk_console.h"
#include "prints.h"

fs::path DuktapeInterface::core_lib_path_;

DuktapeInterface::DuktapeInterface(duk_context *context):context_(context) {
    init();
}

DuktapeInterface::DuktapeInterface():DuktapeInterface(0) {
}

DuktapeInterface::~DuktapeInterface() {
  if( context_ != 0) {
    duk_destroy_heap(context_);
    context_ = 0;
  }
}

void DuktapeInterface::init() {
  std::lock_guard<recursive_mutex> duktape_lock(getMutex());

  if(!getContext()){
    context_ = duk_create_heap(NULL, NULL, NULL, this, custom_fatal_handler);
  }

  DBOUT(__func__ << "(): adding Log path");
  DukTapeCallbacks::addLogPath(getContext(), getContextPath());

  DBOUT(__func__ << "(): register print alert");
  registerPrintAlert();

  DBOUT(__func__ << "(): initialize duktape console");
  duk_console_init(getContext(), DUK_CONSOLE_PROXY_WRAPPER /*flags*/);

  DBOUT(__func__ << "(): initializing node.js require");
  registerNodeModuleFuncs();

}

void DuktapeInterface::registerPrintAlert() {
  std::lock_guard<recursive_mutex> duktape_lock(getMutex());

  duk_push_global_object(getContext());
  duk_push_string(getContext(), "print");
  duk_push_c_function(getContext(), DukTapeCallbacks::cb_print, DUK_VARARGS);
  duk_def_prop(getContext(), -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_CONFIGURABLE);
  duk_push_string(getContext(), "alert");
  duk_push_c_function(getContext(), DukTapeCallbacks::cb_alert, DUK_VARARGS);
  duk_def_prop(getContext(), -3, DUK_DEFPROP_HAVE_VALUE | DUK_DEFPROP_SET_WRITABLE | DUK_DEFPROP_SET_CONFIGURABLE);
  duk_pop(getContext());
}

void DuktapeInterface::registerNodeModuleFuncs() {
  std::lock_guard<recursive_mutex> duktape_lock(getMutex());

  // initializing node.js require
  duk_push_object(getContext());
  DBOUT ("Pushing resolve module");
  duk_push_c_function(getContext(), DukTapeCallbacks::cb_resolve_module, DUK_VARARGS);
  duk_put_prop_string(getContext(), -2, "resolve");
  DBOUT ("Pushing load module");
  duk_push_c_function(getContext(), DukTapeCallbacks::cb_load_module, DUK_VARARGS);
  duk_put_prop_string(getContext(), -2, "load");
  duk_module_node_init(getContext());
}
