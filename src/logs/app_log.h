#ifndef APP_LOG_H_INCLUDED
#define APP_LOG_H_INCLUDED

#include <ostream>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>

using std::string;

class AppLog : public std::streambuf, public std::ostream {
  public:
    AppLog(const char* path) : std::ostream(this) {
      // std::ofstream(path)
      string tmp = path;
      tmp += "/logs.txt";
      file_.open(tmp.c_str(), std::ofstream::app);
      path_ = path;
    }

    ~AppLog() {
      file_.close();
    }

    std::streambuf::int_type overflow(std::streambuf::int_type c)
    {
      file_ << (char)c;
      return 0;
    }

    static string getTimeStamp() {
      std::stringstream tmp;
      std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
      std::time_t now_c = std::chrono::system_clock::to_time_t(now);

      tmp << std::put_time(std::localtime(&now_c), "[%a %b %d %H:%M:%S %Y]");

      return tmp.str();
    }


  private:
    const char* path_;
    std::ofstream file_;

};

#endif
