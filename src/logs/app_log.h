#ifndef APP_LOG_H_INCLUDED
#define APP_LOG_H_INCLUDED

#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>

using std::string;

#include "constant.h"

class AppLog : public std::streambuf, public std::ostream {
  public:
    AppLog(const char* path) : std::ostream(this) {
      // std::ofstream(path)
      string tmp = path;
      tmp += Constant::Paths::LOG_PATH;
      getFile().open(tmp.c_str(), std::ofstream::app);
      setPath(path);
    }

    ~AppLog() {
      getFile().close();
    }

    std::streambuf::int_type overflow(std::streambuf::int_type c)
    {
      getFile() << (char)c;
      return 0;
    }

    static string getTimeStamp() {
      std::stringstream tmp;
      std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
      std::time_t now_c = std::chrono::system_clock::to_time_t(now);

      tmp << std::put_time(std::localtime(&now_c), Constant::String::LOG_TIMESTAMP);

      return tmp.str();
    }

    /* ******************* */
    /* Setters and Getters */
    /* ******************* */

    void setPath(const char* path) { path_ = path; }

    inline std::ofstream& getFile() { return file_; }


  private:
    const char* path_;
    std::ofstream file_;

};

#endif
