#include "response.h"

#include <iostream>
#include <vector>

using namespace std;

#include <sys/fcntl.h>
#include <errno.h>

#include "file_ops.h"
#include "application.h"

int handle_404_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  dest_buffer->len = lws_snprintf(dest_buffer->str, sizeof(dest_buffer->str),
      "REST API was not found");

  if (lws_add_http_common_headers(wsi, 404, "text/html", dest_buffer->len, &p, end))
    return 1;
  if (lws_finalize_write_http_header(wsi, start, &p, end))
    return 1;

  lws_callback_on_writable(wsi);

  return 0;
}

int handle_http_GET_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  dest_buffer->len = lws_snprintf(dest_buffer->str, sizeof(dest_buffer->str),
      "<html>"
      "<h1>GET request</h1>"
      "</html>");

  if(write_GET_response_headers(wsi, dest_buffer, start, p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

int write_GET_response_headers(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;
  /* prepare and write http headers */
  if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
          "text/html", dest_buffer->len, &p, end))
    return 1;
  if (lws_finalize_write_http_header(wsi, start, &p, end))
    return 1;

  return false;
}

int handle_http_POST_header(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  if(write_POST_response_headers(wsi, dest_buffer, start, p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

int handle_http_POST_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  map<duk_context*, JSApplication*> apps = JSApplication::getApplications();
  JSApplication *app = 0;

  for (  map<duk_context*, JSApplication*>::const_iterator it=apps.begin(); it!=apps.end(); ++it) {
    // if the application already exists, simply assign it
    if(it->second->getAppPath() + "/" == "applications/" + string(dest_buffer->ext_filename)) {
      app = it->second;
      break;
    }
  }

  // creating new one if not found on previous
  if(!app) {
    string temp_path = "applications/" + string(dest_buffer->ext_filename);
    app = new JSApplication(temp_path.c_str());
  } else {
    // we have to reload the app if it's already running
    while(!app->getMutex()->try_lock()) {}
    app->getMutex()->unlock();
    app->reload();
  }

  dest_buffer->large_str = app->getDescriptionAsJSON();

  dest_buffer->len = dest_buffer->large_str.length();
  //
  // dest_buffer->len = lws_snprintf(dest_buffer->str, sizeof(dest_buffer->str),
  //     "<html>"
  //     "<h1>POST request yatta</h1>"
  //     "<h1>POST request yatta</h1>"
  //     "<h1>POST request yatta</h1>"
  //     "<h1>POST request yatta</h1>"
  //     "<h1>POST request yatta</h1>"
  //     "<h1>POST request yatta</h1>"
  //     "<h1>POST request yatta</h1>"
  //     "<h1>POST request yatta</h1>"
  //     "<h1>POST request yatta</h1>"
  //     "<h1>POST request yatta</h1>"
  //     "</html>");

  if(write_POST_response_headers(wsi, dest_buffer, start, p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

int write_POST_response_headers(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;
  /* prepare and write http headers */
  if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
          "text/html", dest_buffer->len, &p, end))
    return 1;
  if (lws_finalize_write_http_header(wsi, start, &p, end))
    return 1;

  return false;
}

int handle_http_POST_form(struct lws *wsi, void *buffer_data, void *in, size_t len) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;
  /* create the POST argument parser if not already existing */
  if (!dest_buffer->spa) {
    dest_buffer->spa = lws_spa_create(wsi, post_param_names,
        ARRAY_SIZE(post_param_names), 2048, handle_http_POST_form_filecb, dest_buffer);
    if (!dest_buffer->spa)
      return -1;
  }

  /* let it parse the POST data */
  if (lws_spa_process(dest_buffer->spa, (const char*)in, (int)len)) {
    return -1;
  }

  return false;
}

int handle_http_POST_form_complete(struct lws *wsi, void *buffer_data, void *in, size_t len) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  /* inform the spa no more payload data coming */
  lws_spa_finalize(dest_buffer->spa);

  /* we just dump the decoded things to the log */

  for (int n = 0; n < (int)ARRAY_SIZE(post_param_names); n++) {
    if (!lws_spa_get_string(dest_buffer->spa, n))
      lwsl_user("%s: undefined\n", post_param_names[n]);
    else
      lwsl_user("%s: (len %d) '%s'\n",
          post_param_names[n],
          lws_spa_get_length(dest_buffer->spa, n),
          lws_spa_get_string(dest_buffer->spa, n));
  }

  return -1;
}

int handle_http_POST_form_filecb(void *data, const char *name, const char *filename,
	       char *buf, int len, enum lws_spa_fileupload_states state) {
	struct user_buffer_data *dest_buffer = (struct user_buffer_data *)data;
	int n;

	switch (state) {
  	case LWS_UFS_OPEN: {
  		/* take a copy of the provided filename */
  		lws_strncpy(dest_buffer->filename, filename, sizeof(dest_buffer->filename) - 1);
  		/* remove any scary things like .. */
  		lws_filename_purify_inplace(dest_buffer->filename);

  		/* open a file of that name for write in the cwd */
      std::string path = "tmp/" + std::string(dest_buffer->filename);
  		dest_buffer->fd = open(path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0600);
  		if (dest_buffer->fd == LWS_INVALID_FILE) {
  			lwsl_notice("Failed to open output file %s\n",
  				    dest_buffer->filename);
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
				lwsl_notice("Problem writing file %d\n", errno);
			}
		}
		if (state == LWS_UFS_CONTENT)
			/* wasn't the last part of the file */
			break;

		/* the file upload is completed */

		lwsl_user("%s: upload done, written %lld to %s\n", __func__,
			  dest_buffer->file_length, dest_buffer->filename);

		close(dest_buffer->fd);
    dest_buffer->fd = LWS_INVALID_FILE;

    // we have the file in file system now so we can just extract it
    std::string ext_filename = extract_file(std::string("tmp/" + std::string(dest_buffer->filename)).c_str(), "tmp/test");

    strcpy(dest_buffer->ext_filename,ext_filename.c_str());

    lwsl_user("%s: extracted, written to applicaitons/%s\n", __func__, dest_buffer->ext_filename);

		break;
	}

	return 0;
}

int handle_http_DELETE_response(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;

  dest_buffer->len = lws_snprintf(dest_buffer->str, sizeof(dest_buffer->str),
      "<html>"
      "<h1>DELETE request</h1>"
      "</html>");

  if(write_DELETE_response_headers(wsi, dest_buffer, start, p, end)) {
    return 1;
  }

  lws_callback_on_writable(wsi);

  return 0;
}

int write_DELETE_response_headers(struct lws *wsi, void* buffer_data, uint8_t *start, uint8_t *p, uint8_t *end) {
  struct user_buffer_data *dest_buffer = (struct user_buffer_data*)buffer_data;
  /* prepare and write http headers */
  if (lws_add_http_common_headers(wsi, HTTP_STATUS_OK,
          "text/html", dest_buffer->len, &p, end))
    return 1;
  if (lws_finalize_write_http_header(wsi, start, &p, end))
    return 1;

  return false;
}
