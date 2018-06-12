#ifndef READ_APP_LOG_H_INCLUDED
#define READ_APP_LOG_H_INCLUDED

#include <istream>
#include <fstream>
#include <string>
#include <sstream>

#include <iostream>

using std::string;

class ReadAppLog {
  public:
    ReadAppLog(const char* path) {
      // std::ofstream(path)
      string tmp = path;
      tmp += "/logs.txt";
      file_.open(tmp.c_str());
      path_ = path;
    }

    ~ReadAppLog() {
      file_.close();
    }

    string getLogsString()
    {
      std::string str((std::istreambuf_iterator<char>(file_)),
                 std::istreambuf_iterator<char>());

      return str;
    }

  private:
    const char* path_;
    std::ifstream file_;
};

#endif
