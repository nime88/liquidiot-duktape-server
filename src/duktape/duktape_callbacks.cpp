#include "duktape_callbacks.h"

#include "duktape.h"

#include <vector>
using std::vector;

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include "app_log.h"
#include "prints.h"
#include "constant.h"
#include "file_ops.h"
#include "util.h"
#include "node_module_search.h"

vector<std::pair<duk_context*,fs::path> > DukTapeCallbacks::log_paths_;
fs::path DukTapeCallbacks::core_lib_path_;

duk_ret_t DukTapeCallbacks::cb_resolve_module(duk_context *ctx) {
    /*
     *  Entry stack: [ requested_id parent_id ]
     */

    const char *requested_id;
    const char *parent_id;
    (void)parent_id;
    fs::path app_path = getLogPath(ctx);

    try {
      requested_id = duk_require_string(ctx, 0);
      parent_id = duk_require_string(ctx, 1);
    } catch( ... ) {
      ERROUT(__func__ << "(): " << duk_safe_to_string(ctx, -1));
      AppLog(app_path.c_str()) << AppLog::getTimeStamp() << " [" << Constant::String::LOG_ERROR << "] " << duk_safe_to_string(ctx, -1) << "\n";
      return 0;
    }

    duk_pop_2(ctx);
    /* [ ... ] */

    string requested_id_str = string(requested_id);
    // trying to "purify" relative paths
    requested_id_str.erase(std::remove(requested_id_str.begin()+1, requested_id_str.end()-3, '.'), requested_id_str.end()-3);

    // if buffer is requested just use native duktape implemented buffer
    if(requested_id_str == "buffer") {
      duk_push_string(ctx, requested_id);
      return 1;  /*nrets*/
    }

    if(requested_id_str.size() > 0) {
      // First checking core modules
      if (requested_id_str.at(0) != '.' && requested_id_str.at(0) != '/') {
        if(!getCoreLibPath().empty()) {
          fs::path module_path = getCoreLibPath();
          module_path /= fs::path(requested_id);
          if (node::is_core_module(module_path.string())) {
            requested_id_str = node::get_core_module(module_path.string());
            duk_push_string(ctx, requested_id_str.c_str());
            return 1;
          } else if(is_file(module_path.c_str()) == FILE_TYPE::PATH_TO_DIR) {
            duk_push_string(ctx, module_path.c_str());
            return 1;
          }
        }
      }

      // Then checking if path is system root
      if(requested_id_str.at(0) == '/') {
        app_path = "";
      }

      if(requested_id_str.substr(0,2) == "./" || requested_id_str.at(0) ==  '/' || requested_id_str.substr(0,3) == "../") {
        fs::path local_file = app_path;
        local_file /= fs::path(requested_id);
        if(is_file(local_file.c_str()) == FILE_TYPE::PATH_TO_FILE ||
          is_file(local_file.c_str()) == FILE_TYPE::PATH_TO_DIR) {
          requested_id_str = local_file.string();
        }
      }
    }

    duk_push_string(ctx, requested_id_str.c_str());

    /* [ ... resolved_id ] */

  return 1;  /*nrets*/
}

duk_ret_t DukTapeCallbacks::cb_load_module(duk_context *ctx) {
    /*
     *  Entry stack: [ resolved_id exports module ]
     */

    const char *resolved_id = duk_require_string(ctx, 0);
    duk_get_prop_string(ctx, 2, "filename");
    const char *filename = duk_require_string(ctx, -1);
    (void)filename;

    if (string(resolved_id) == "buffer") {
      duk_push_string(ctx,"module.exports.Buffer = Buffer;");
      return 1;
    }

    char *module_source = 0;

    // if resolved id is an actual file we try to load it as javascript
    if(is_file(resolved_id) == FILE_TYPE::PATH_TO_FILE) {
      module_source = node::load_as_file(resolved_id);
      if(!module_source) {
        (void) duk_type_error(ctx, "cannot find module: %s", resolved_id);
        return 1;
      }
    }
    // if resolved to directory we try to load it as such
    else if(is_file(resolved_id) == FILE_TYPE::PATH_TO_DIR) {
      // first we have to read the package.json
      string path = string(resolved_id);
      path = path + "package.json";

      if(is_file(path.c_str()) == FILE_TYPE::PATH_TO_FILE) {
        int sourceLen;
        char* package_js = load_js_file(path.c_str(),sourceLen);

        map<string,string> json_attr = read_package_json(ctx, package_js);
        delete package_js;
        package_js = 0;

        // loading main file to source
        path = string(resolved_id) + json_attr.at(Constant::Attributes::APP_MAIN);
        module_source = node::load_as_file(path.c_str());
        if(!module_source) {
          (void) duk_type_error(ctx, "cannot find module: %s", resolved_id);
          return 1;
        }
      }
    }

    if(!module_source) {
      (void) duk_type_error(ctx, "cannot find module: %s", resolved_id);
      return 1;
    }

    string source = module_source;

    if(source.size() > 0) {
      // pushing the source code to heap as the require system wants that
      duk_push_string(ctx, module_source);
    } else {
      (void) duk_type_error(ctx, "cannot find module: %s", resolved_id);
    }

  return 1;  /*nrets*/
}

duk_ret_t DukTapeCallbacks::cb_print (duk_context *ctx) {
  duk_idx_t nargs = duk_get_top(ctx);

  duk_push_string(ctx, " ");
  duk_insert(ctx, 0);
  duk_concat(ctx, nargs);

  fs::path app_path = DukTapeCallbacks::getLogPath(ctx);
  // writing to log
  try {
    AppLog(app_path.c_str()) << AppLog::getTimeStamp() << " [" << Constant::String::LOG_PRINT << "] " << duk_require_string(ctx, -1) << "\n";
    duk_pop(ctx);
  } catch ( ... ) {
    ERROUT(__func__ << "(): Error writing to logs");
  }
  duk_pop(ctx);

  return 0;
}

duk_ret_t DukTapeCallbacks::cb_alert (duk_context *ctx) {
  duk_idx_t nargs = duk_get_top(ctx);

  duk_push_string(ctx, " ");
  duk_insert(ctx, 0);
  duk_concat(ctx, nargs);

  fs::path app_path = DukTapeCallbacks::getLogPath(ctx);
  // writing to log
  try {
    AppLog(app_path.c_str()) << AppLog::getTimeStamp() << " [" << Constant::String::LOG_ALERT << "] " << duk_require_string(ctx, -1) << "\n";
    duk_pop(ctx);
  } catch (...) {
    ERROUT(__func__ << "(): Error writing to logs");
  }

  return 0;
}
