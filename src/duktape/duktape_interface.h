// Instantiable interface class for duktape operations

#ifndef DUKTAPE_INTERFACE_H
#define DUKTAPE_INTERFACE_H

#include <mutex>
#include <boost/filesystem.hpp>

using std::recursive_mutex;
namespace fs = boost::filesystem;

#if defined (__cplusplus)
extern "C" {
#endif

#include "duk_print_alert.h"
#include "duk_module_node.h"

#if defined (__cplusplus)
}
#endif

#include "duktape.h"

#include "duktape_callbacks.h"

class DuktapeInterface {
  public:
    DuktapeInterface();
    DuktapeInterface(duk_context *context);

    ~DuktapeInterface();

    inline duk_context* getContext() { return context_; }
    inline void setContextPath(const fs::path &cpath) {
      context_path_ = cpath;
      DukTapeCallbacks::addLogPath(getContext(), getContextPath());
    }
    inline const fs::path& getContextPath() { return context_path_; }
    static inline void setCoreLibPath(const fs::path &cpath) {
      core_lib_path_ = cpath;
      DukTapeCallbacks::setCoreLibPath(core_lib_path_);
    }
    static inline const fs::path& getCoreLibPath() { return core_lib_path_; }

    DuktapeInterface(DuktapeInterface const&) = delete;
    void operator=(DuktapeInterface const&) = delete;
  private:
    void init();
    inline recursive_mutex& getMutex() { return mutex_; }

    void registerPrintAlert();
    void registerNodeModuleFuncs();

    duk_context *context_;
    recursive_mutex mutex_;
    fs::path context_path_;
    static fs::path core_lib_path_;
};

#endif
