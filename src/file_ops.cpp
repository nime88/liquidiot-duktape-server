#include "file_ops.h"

#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include <archive.h>
#include <archive_entry.h>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using std::cout;
using std::ifstream;
using std::pair;
using std::istringstream;

#include "prints.h"
#include "constant.h"

char* load_js_file(const char* filename, int & sourceLen) {
  char* sourceCode;

  DBOUT ( "Filename: " << filename );

  ifstream file(filename);
  string content( (std::istreambuf_iterator<char>(file) ),
                     (std::istreambuf_iterator<char>()    ) );

  // converting c++ string to char* source code for return
  sourceLen = content.length() + 1;
  sourceCode = new char [sourceLen];
  // updating the source code length
  strcpy(sourceCode, content.c_str());

  if (file.is_open()) file.close();

  if(sourceLen <= 1) {
    DBOUT ( "File " << filename << " doesn't exist or is empty." );
  }

  return sourceCode;
}

map<string, string> load_config(const char* filename) {
  ifstream file(filename);
  string content( (std::istreambuf_iterator<char>(file) ),
                     (std::istreambuf_iterator<char>()    ) );

  istringstream is_file(content);

  string line;

  map<string, string> options;

  while( getline(is_file, line) ) {
    istringstream is_line(line);
    string key;
    if( getline(is_line, key, '=') ) {
      string value;
      if( getline(is_line, value) )
        options.insert(pair<string,string>(key,value));
    }
  }

  return options;

}

FILE_TYPE is_file(const char* fpath) {
  fs::path bpath(fpath);
  if(fs::is_directory(bpath)) return FILE_TYPE::PATH_TO_DIR;

  if(fs::is_regular_file(bpath)) return FILE_TYPE::PATH_TO_FILE;

  if(fs::is_other(bpath)) return FILE_TYPE::OTHER;

  return FILE_TYPE::NOT_EXIST;
}

string extract_file(const char* file_path, const char* extract_path, const char* filename) {
  INFOOUT("File path: " << file_path);
  INFOOUT("Ext path: " << extract_path);
  int flags;

  flags = ARCHIVE_EXTRACT_TIME;

  return extract(file_path, 1, flags, extract_path, filename);
}

string extract(const char *in_filename, int do_extract, int flags, const char* extract_path, const char* out_filename)
{
	struct archive *a;
	struct archive *ext;
	struct archive_entry *entry;
	int r;
  string final_filename = "";

	a = archive_read_new();
	ext = archive_write_disk_new();
	archive_write_disk_set_options(ext, flags);

  archive_read_support_filter_gzip(a);
	archive_read_support_format_tar(a);

	if (in_filename != NULL && strcmp(in_filename, "-") == 0)
		in_filename = NULL;
	if ((r = archive_read_open_filename(a, in_filename, 10240))) {
    DBOUT ( "archive_read_open_filename()" );
    DBOUT ( archive_error_string(a) );
		return "";
  }

	for (;;) {
		r = archive_read_next_header(a, &entry);

		if (r == ARCHIVE_EOF)
			break;

		if (r != ARCHIVE_OK) {
      DBOUT ( "archive_read_next_header()" );
      DBOUT ( archive_error_string(a) );
      return "";
    }

		if (do_extract) {
      // setting extract path
      const char* currentFile = archive_entry_pathname(entry);

      // storing the extract path for later use
      if(final_filename.length() == 0) {
        fs::path fs_path(currentFile);
        DBOUT(__func__ << ": " << fs_path);
        final_filename = fs_path.parent_path().string();
      }

      string fullOutputPath;

      // separating if we have designated extract path or not
      if(!out_filename)
        fullOutputPath = string(extract_path) + "/" + string(currentFile);
      else {
        fullOutputPath = string(extract_path) + "/" + string(out_filename) + "/" + string(currentFile);
        size_t found;
        found=string(currentFile).find_first_of("/");
        fullOutputPath = string(currentFile).substr(found+1);
        fullOutputPath = string(extract_path) + "/" + string(out_filename) + "/" + fullOutputPath;
      }

      archive_entry_set_pathname(entry, fullOutputPath.c_str());

			r = archive_write_header(ext, entry);

			if (r != ARCHIVE_OK) {
        DBOUT ( "archive_write_header()" );
        DBOUT ( archive_error_string(ext) );
        return "";
      } else {
				copy_data(a, ext);
				r = archive_write_finish_entry(ext);
				if (r != ARCHIVE_OK) {
          DBOUT ( "archive_write_finish_entry()" );
          DBOUT ( archive_error_string(ext) );
          return "";
        }
			}
		}
	}
	archive_read_close(a);
	archive_read_free(a);

	archive_write_close(ext);
  archive_write_free(ext);

  return final_filename;
}

int copy_data(struct archive *ar, struct archive *aw) {
	int r;
	const void *buff;
	size_t size;
#if ARCHIVE_VERSION_NUMBER >= 3000000
	int64_t offset;
#else
	off_t offset;
#endif

	for (;;) {
		r = archive_read_data_block(ar, &buff, &size, &offset);
		if (r == ARCHIVE_EOF)
			return (ARCHIVE_OK);
		if (r != ARCHIVE_OK)
			return (r);
		r = archive_write_data_block(aw, buff, size, offset);
		if (r != ARCHIVE_OK) {
      DBOUT ( "archive_write_data_block()" );
      DBOUT ( archive_error_string(aw) );
			return (r);
		}
	}
}

int delete_files(const char* file_path) {
  DBOUT(__func__ << "(): Deleting path -> " << file_path);
  fs::path bpath(file_path);
  return fs::remove_all(bpath);
}
