#include "post_request.h"

#include "application.h"
#include "file_ops.h"
#include "constant.h"

#include <sys/fcntl.h>
#include <cstdio>

const char * const PostRequest::post_param_names[] = {
  "filekey",
};

int PostRequest::handleHttpRequest(struct lws *wsi, void* buffer_data, void* in, uint8_t *start, uint8_t *p, uint8_t *end, size_t len, enum lws_callback_reasons reason) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  switch (reason) {
    // initial http request (headers)
    case LWS_CALLBACK_HTTP: {
      // storing the request url from the mountpoint
      dest_buffer->request_url = (char*)in;

      return 0;
    }

    case LWS_CALLBACK_HTTP_WRITEABLE: {
      // writing request when asked to
      return writeHttpRequest(wsi, buffer_data, in, len);
    }

    case LWS_CALLBACK_HTTP_BODY: {
      return parsePostForm(wsi, buffer_data, in, len);
    }

    case LWS_CALLBACK_HTTP_BODY_COMPLETION: {
      /* inform the spa no more payload data coming */
      lws_spa_finalize(dest_buffer->spa);

      if(dest_buffer->error_msg.length() > 0) {
        return generateFailResponse(wsi, buffer_data, start, p, end);
      }

      int post_resp = calculateHttpRequest(buffer_data, in);

      if(post_resp < 0) {
        return generateFailResponse(wsi, buffer_data, start, p, end);
      }

      return generateResponse(wsi, buffer_data, start, p, end);
    }

    default:
      break;
  }

  return lws_callback_http_dummy(wsi, reason, buffer_data, in, len);
}

int PostRequest::generateResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK, Constant::String::REQ_TYPE_APP_JSON, dest_buffer->len, &p, end))
    return 1;

  if (lws_finalize_write_http_header(wsi, start, &p, end))
    return 1;

  lws_callback_on_writable(wsi);

  return 0;
}

int PostRequest::generateFailResponse(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  dest_buffer->large_str = "{\"error\":\"" + dest_buffer->error_msg + "\"}";
  dest_buffer->len = dest_buffer->large_str.length();

  /* prepare and write http headers */
  if (lws_add_http_common_headers(wsi, HTTP_STATUS_NOT_FOUND,
          Constant::String::REQ_TYPE_TEXT_HTML, dest_buffer->len, &p, end)) {
    return 1;
  }
  if (lws_finalize_write_http_header(wsi, start, &p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

int PostRequest::parsePostForm(struct lws *wsi, void* buffer_data, void* in, size_t len) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  /* create the POST argument parser if not already existing */
  if (!dest_buffer->spa) {
    dest_buffer->spa = lws_spa_create(wsi, post_param_names,
        ARRAY_SIZE(post_param_names), 2048, PostRequest::parsePostFormCB, dest_buffer);
    if (!dest_buffer->spa) {
      dest_buffer->error_msg = "Failed to create request (probably server side problem)";
      return 0;
    }
  }

  /* let it parse the POST data */
  if (lws_spa_process(dest_buffer->spa, (const char*)in, (int)len)) {
    if(dest_buffer->error_msg.length() > 0) {
      return 0;
    }
    return 1;
  }

  return 0;
}

int PostRequest::parsePostFormCB(void *data, const char *name, const char *filename, char *buf, int len, enum lws_spa_fileupload_states state) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data *)data;
  int n;

  if(string(name) != string(post_param_names[post_enum_param_names::EPN_FILEKEY])) {
    ERROUT("Unexpected key " << name);
    dest_buffer->error_msg = "Unidentified '" + string(name) + "' parameter";
    return -1;
  }

  switch (state) {
    case LWS_UFS_OPEN: {
      /* take a copy of the provided filename */
      lws_strncpy(dest_buffer->filename, filename, sizeof(dest_buffer->filename) - 1);
      /* remove any scary things like .. */
      lws_filename_purify_inplace(dest_buffer->filename);

      /* open a file of that name for write in the cwd */
      std::string path = Device::getInstance().getExecPath() + "/" + string(Constant::Paths::TEMP_FOLDER) + "/" + std::string(dest_buffer->filename);
      dest_buffer->fd = open(path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0666);
      if (dest_buffer->fd == LWS_INVALID_FILE) {
        ERROUT("Failed to open output file " << dest_buffer->filename);
        dest_buffer->error_msg = "Failed to open output file '" + string(dest_buffer->filename) + "'";
        return -1;
      }
      break;
    }
  case LWS_UFS_FINAL_CONTENT:
  case LWS_UFS_CONTENT:
    if (len) {
      dest_buffer->file_length += len;

      n = write(dest_buffer->fd, buf, len);
      if (n < len) {
        ERROUT("Problem writing file " << errno);
      }
    }
    if (state == LWS_UFS_CONTENT)
      /* wasn't the last part of the file */
      break;

    /* the file upload is completed */
    DBOUT(__func__ << ": upload done, written " << dest_buffer->file_length << " to " << dest_buffer->filename);

    close(dest_buffer->fd);
    dest_buffer->fd = LWS_INVALID_FILE;

    break;
  }

  return 0;
}

int PostRequest::calculateHttpRequest(void* buffer_data, void* in) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  map<duk_context*, JSApplication*> apps = JSApplication::getApplications();
  JSApplication *app = 0;

  (void)in;

  int id = parseIdFromURL(dest_buffer->request_url);

  if(id == -1) {
    string temp_file = Device::getInstance().getExecPath() + "/" + string(string(Constant::Paths::TEMP_FOLDER) + "/" + string(dest_buffer->filename));
    string temp_path = Device::getInstance().getExecPath() + "/" + Constant::Paths::APPLICATIONS_ROOT;
    // we have the file in file system now so we can just extract it
    string ext_filename = extract_file(temp_file.c_str(), temp_path.c_str(), 0);
    strcpy(dest_buffer->ext_filename,ext_filename.c_str());
    DBOUT(__func__ << ": extracted, written to " << Constant::Paths::APPLICATIONS_ROOT << "/" << dest_buffer->ext_filename);

    if( is_file(temp_file.c_str()) == FILE_TYPE::PATH_TO_FILE && remove( temp_file.c_str() ) != 0 ) {
      ERROUT("Failed to delete file: " << temp_file); }
    else { DBOUT("Deleted file " << temp_file); };
  }

  if(id >= 0) {
    string temp_file = Device::getInstance().getExecPath() + "/" + string(Constant::Paths::TEMP_FOLDER) + "/" + string(dest_buffer->filename);

    for (  map<duk_context*, JSApplication*>::const_iterator it=apps.begin(); it!=apps.end(); ++it) {
      // if the application already exists, simply assign it
      if(id == it->second->getAppId()) {
        app = it->second;
        // copying contents to updated app
        string ext_filename = extract_file(temp_file.c_str(), app->getAppPath().c_str(), "");
        strcpy(dest_buffer->ext_filename,ext_filename.c_str());
        DBOUT(__func__ << ": extracted, written to " << Constant::Paths::APPLICATIONS_ROOT << "/" << dest_buffer->ext_filename);

        if( is_file(temp_file.c_str()) == FILE_TYPE::PATH_TO_FILE && remove( temp_file.c_str() ) != 0 ) {
          ERROUT("Failed to delete file: " << temp_file); }
        else { DBOUT("Deleted file " << temp_file); };

        break;
      }
    }
  } else if(id == -1) {
    for (  map<duk_context*, JSApplication*>::const_iterator it=apps.begin(); it!=apps.end(); ++it) {
      // if the application already exists, simply assign it
      if(it->second->getAppPath() == string(Constant::Paths::APPLICATIONS_ROOT) + "/" + string(dest_buffer->ext_filename)) {
        app = it->second;
        break;
      }
    }
  }

  // creating new one if not found on previous
  if(!app && id == -1) {
    string temp_path = string(Constant::Paths::APPLICATIONS_ROOT) + "/" + string(dest_buffer->ext_filename);
    app = new JSApplication(temp_path.c_str());
  } else if(app) {
    // we have to reload the app if it's already running
    DBOUT("Reloading app");
    string temp_path = app->getAppPath().c_str();
    JSApplication::shutdownApplication(app);
    app = new JSApplication(temp_path.c_str());
  }

  if(!app) {
    dest_buffer->error_msg = "Id '" + dest_buffer->request_url + "' not found.";
    return -1;
  }

  dest_buffer->large_str = app->getDescriptionAsJSON();
  dest_buffer->len = dest_buffer->large_str.length();

  return 0;
}
