#ifndef DUKTAPE_CALLBACKS_H
#define DUKTAPE_CALLBACKS_H

#include "duktape.h"

#include <vector>
using std::vector;

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

class DukTapeCallbacks {
  public:
    static duk_ret_t cb_resolve_module(duk_context *ctx);
    static duk_ret_t cb_load_module(duk_context *ctx);
    static duk_ret_t cb_print(duk_context *ctx);
    static duk_ret_t cb_alert(duk_context *ctx);

    static inline void addLogPath(duk_context *ctx, const fs::path &log_path ) {
      for( auto &pr : log_paths_ ) {
        if(pr.first == ctx) {
          pr.second = log_path;
          return;
        }
      }
      log_paths_.push_back(std::pair<duk_context*,fs::path>(ctx,log_path));
    }

    static inline fs::path getLogPath(duk_context *ctx) {
      for(const auto &pr : log_paths_) {
        if(pr.first == ctx) return pr.second;
      }

      return fs::path();
    }

    static inline void setCoreLibPath(const fs::path clib_path) { core_lib_path_ = clib_path; }
    static inline const fs::path& getCoreLibPath() { return core_lib_path_; }

  private:
    DukTapeCallbacks();

    static vector<std::pair<duk_context*,fs::path> > log_paths_;
    static fs::path core_lib_path_;

};

#endif
