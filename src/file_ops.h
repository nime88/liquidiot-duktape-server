#ifndef FILE_OPS_H
#define FILE_OPS_H

#include <map>
#include <string>

enum FILE_TYPE {
  PATH_TO_DIR,PATH_TO_FILE,OTHER,NOT_EXIST
};

extern char* load_js_file(const char* filename, int & sourceLen);

// Loads file as config
std::map<std::string, std::string> load_config(const char* filename);

FILE_TYPE is_file(const char* path);

// extracts file from location to
extern std::string extract_file(const char* file_path, const char* extract_path);
extern std::string extract(const char *filename, int do_extract, int flags);
extern int copy_data(struct archive *ar, struct archive *aw);

#endif
