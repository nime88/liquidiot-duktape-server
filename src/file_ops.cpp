#include "file_ops.h"

#include <iostream>
#include <cstring>
#include <fstream>
#include <ios>
#include <sstream>
#include <map>
#include <sys/stat.h>
#include <archive.h>
#include <archive_entry.h>

using namespace std;

char* load_js_file(const char* filename, int & sourceLen) {
  char* sourceCode;

  cout << "Filename: " << filename << endl;

  ifstream file(filename);
  std::string content( (std::istreambuf_iterator<char>(file) ),
                     (std::istreambuf_iterator<char>()    ) );

  // converting c++ string to char* source code for return
  sourceLen = content.length() + 1;
  sourceCode = new char [sourceLen];
  // updating the source code length
  strcpy(sourceCode, content.c_str());

  if (file.is_open()) file.close();

  if(sourceLen <= 1) {
    cout << "File " << filename << " doesn't exist or is empty." << endl;
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

FILE_TYPE is_file(const char* path) {
  struct stat s;

  if( stat(path,&s) == 0 ) {
    if( s.st_mode & S_IFDIR ) {
      //it's a directory
      // TODO later we can search the directory
      return FILE_TYPE::PATH_TO_DIR;
    } else if( s.st_mode & S_IFREG ) {
      //it's a file
      return FILE_TYPE::PATH_TO_FILE;
    } else {
      // std::cout << "What is this" << std::endl;
      return FILE_TYPE::OTHER;
    }
  } else {
    //error
    return FILE_TYPE::NOT_EXIST;
  }

  return FILE_TYPE::NOT_EXIST;
}

int extract_file(const char* file_path, const char* extract_path) {
  int compress, flags, mode, opt, verbose;

  flags = ARCHIVE_EXTRACT_TIME;

  extract(file_path, 1, flags);

  return 0;
}

void extract(const char *filename, int do_extract, int flags)
{
	struct archive *a;
	struct archive *ext;
	struct archive_entry *entry;
	int r;

	a = archive_read_new();
	ext = archive_write_disk_new();
	archive_write_disk_set_options(ext, flags);

	/*
	 * Note: archive_write_disk_set_standard_lookup() is useful
	 * here, but it requires library routines that can add 500k or
	 * more to a static executable.
	 */
  archive_read_support_compression_gzip(a);
	archive_read_support_format_tar(a);
	/*
	 * On my system, enabling other archive formats adds 20k-30k
	 * each.  Enabling gzip decompression adds about 20k.
	 * Enabling bzip2 is more expensive because the libbz2 library
	 * isn't very well factored.
	 */
	if (filename != NULL && strcmp(filename, "-") == 0)
		filename = NULL;
	if ((r = archive_read_open_filename(a, filename, 10240))) {
    std::cout << "archive_read_open_filename()" << std::endl;
    std::cout << archive_error_string(a) << std::endl;
		return;
  }
	for (;;) {
		r = archive_read_next_header(a, &entry);

		if (r == ARCHIVE_EOF)
			break;

		if (r != ARCHIVE_OK) {
      std::cout << "archive_read_next_header()" << std::endl;
      std::cout << archive_error_string(a) << std::endl;
      return;
    }

		if (do_extract)
      write(1, "x ", strlen("x "));

		if (do_extract) {
      // setting extract path
      const char* currentFile = archive_entry_pathname(entry);
      const std::string fullOutputPath = "applications/" + string(currentFile);
      archive_entry_set_pathname(entry, fullOutputPath.c_str());

			r = archive_write_header(ext, entry);

			if (r != ARCHIVE_OK) {
        std::cout << "archive_write_header()" << std::endl;
        std::cout << archive_error_string(ext) << std::endl;
        return;
      } else {
				copy_data(a, ext);
				r = archive_write_finish_entry(ext);
				if (r != ARCHIVE_OK) {
          std::cout << "archive_write_finish_entry()" << std::endl;
          std::cout << archive_error_string(ext) << std::endl;
          return;
        }
			}
		}
	}
	archive_read_close(a);
	archive_read_free(a);

	archive_write_close(ext);
  archive_write_free(ext);

  return;
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
      std::cout << "archive_write_data_block()" << std::endl;
      std::cout << archive_error_string(aw) << std::endl;
			return (r);
		}
	}
}
