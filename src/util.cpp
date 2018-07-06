#include "util.h"

#include "constant.h"
using std::pair;

#include <fstream>

#include <iostream>
using std::cout;
using std::endl;

map<string, map<string,string> >get_config(duk_context *ctx) {
  map<string, map<string,string> > config;

  std::ifstream file(Constant::CONFIG_PATH);
  string content( (std::istreambuf_iterator<char>(file) ),
                     (std::istreambuf_iterator<char>() ) );

  // decoding source
  duk_push_string(ctx, content.c_str());
  duk_json_decode(ctx, -1);
  /* [... dec_obj ] */

  duk_enum(ctx, -1, DUK_ENUM_OWN_PROPERTIES_ONLY);

  while (duk_next(ctx, -1 /*enum_idx*/, 1)) {
    /* [ ... dec_obj enum main_key value_obj ] */
    if(duk_check_type(ctx, -1, DUK_TYPE_OBJECT)) {
      string main_key = duk_to_string(ctx, -2);
      map<string,string> local_config;

      duk_enum(ctx, -1, DUK_ENUM_OWN_PROPERTIES_ONLY);
      /* [ ... dec_obj enum main_key value_obj value_obj_enum] */
      while (duk_next(ctx, -1 /*enum_idx*/, 1)) {
        /* [ ... enum main_key value_obj value_obj_enum key value ] */
        if(duk_check_type(ctx, -1, DUK_TYPE_STRING)) {
          string key = duk_to_string(ctx, -2);
          string value = duk_to_string(ctx, -1);
          local_config.insert(pair<string,string>(key,value));
        }
        duk_pop_2(ctx); /* pop key value  */
      }
      duk_pop(ctx);
      config.insert(pair<string,map<string,string> >(main_key,local_config));
    }
    duk_pop_2(ctx);  /* pop main_key value_obj_enum  */
  }
  duk_pop_2(ctx); // popping enum object and json object

  /* [...] */

  return config;
}

void save_config(duk_context *ctx, map<string, map<string,string> > new_config) {
  /* [...] */
  duk_push_object(ctx);

  for(map<string, map<string,string> >::iterator out_it = new_config.begin(); out_it != new_config.end(); out_it++) {
      /* [... conf_obj ] */
      int in_idx = duk_push_object(ctx);
      for(map<string,string>::iterator inner_it = out_it->second.begin(); inner_it != out_it->second.end(); ++inner_it) {
        /* [... conf_obj ] */
        duk_push_string(ctx, inner_it->second.c_str());
        duk_put_prop_string(ctx, in_idx, inner_it->first.c_str());
      }
      duk_put_prop_string(ctx, -2, out_it->first.c_str());
  }

  string full_config = duk_json_encode(ctx, -1);
  duk_pop(ctx);

  std::ofstream dev_file;
  dev_file.open (Constant::CONFIG_PATH);
  dev_file << full_config;
  dev_file.close();
}

map<string,string> read_package_json(duk_context *ctx, const char* js_src) {
  if(!ctx || string(js_src).length() == 0) return map<string,string>();

  map<string,string> attr;

  // decoding source
  duk_push_string(ctx, js_src);
  duk_json_decode(ctx, -1);
  /* [... dec_obj ] */

  duk_enum(ctx, -1, DUK_ENUM_OWN_PROPERTIES_ONLY);
  /* [... dec_obj obj_enum ] */

  while (duk_next(ctx, -1 /*enum_idx*/, 1)) {
    /* [ ... dec_obj obj_enum  key value ] */
    if(duk_check_type(ctx, -1, DUK_TYPE_STRING)) {
      string key = duk_to_string(ctx, -2);
      string value = duk_to_string(ctx, -1);
      attr.insert(pair<string,string>(key,value));
    } else if(duk_check_type(ctx, -1, DUK_TYPE_NUMBER)) {
      string key = duk_to_string(ctx, -2);
      int value = duk_to_number(ctx, -1);
      attr.insert(pair<string,string>(key,std::to_string(value)));
    }
    duk_pop_2(ctx);
  }
  duk_pop_2(ctx);

  printf("read_package_json(: OK)\n");
  return attr;
}

map<string,vector<string> > read_liquidiot_json(duk_context *ctx, const char* js_src) {
  if(!ctx || string(js_src).length() == 0) return map<string,vector<string> >();

  map<string,vector<string> > attr;
  vector<string> dc_vec;
  vector<string> ai_vec;

  // decoding source
  duk_push_string(ctx, js_src);
  duk_json_decode(ctx, -1);
  /* [... dec_obj ] */

  if(duk_has_prop_string(ctx, -1, Constant::Attributes::LIOT_DEV_CAP)) {
    duk_get_prop_string(ctx, -1, Constant::Attributes::LIOT_DEV_CAP);
    /* [... dec_obj prop_str ] */

    duk_idx_t index = 0;
    while(duk_get_prop_index(ctx,-1,index)) {
      /* [... dec_obj prop_str prop_str_inner ] */
      dc_vec.push_back(duk_to_string(ctx,-1));
      duk_pop(ctx);
      index++;
    }

    duk_pop(ctx);
    /* [... dec_obj ] */

    attr.insert(pair<string,vector<string> >(Constant::Attributes::LIOT_DEV_CAP, dc_vec));
  }

  duk_pop(ctx);

  if(duk_has_prop_string(ctx, -1, Constant::Attributes::LIOT_APP_INTERFACES)) {
    duk_get_prop_string(ctx, -1, Constant::Attributes::LIOT_APP_INTERFACES);
    /* [... dec_obj prop_str ] */

    duk_idx_t index = 0;
    while(duk_get_prop_index(ctx,-1,index)) {
      /* [... dec_obj prop_str prop_str_inner ] */
      ai_vec.push_back(duk_to_string(ctx,-1));
      duk_pop(ctx);
      index++;
    }

    duk_pop(ctx);

    attr.insert(pair<string,vector<string> >(Constant::Attributes::LIOT_APP_INTERFACES, ai_vec));
  }

  duk_pop_2(ctx);

  return attr;
}

// trying to read raw data to json
map<string,string> read_raw_json(duk_context *ctx, const char* js_src, map<string,string> &attr) {
  // decoding source
  duk_push_string(ctx, js_src);
  duk_json_decode(ctx, -1);
  /* [ ... json_obj ] */

  duk_enum(ctx, -1, DUK_ENUM_OWN_PROPERTIES_ONLY);

  while (duk_next(ctx, -1 /*enum_idx*/, 1)) {
    /* [ ... enum key value ] */
    // only adding strings
    if(duk_check_type(ctx, -1, DUK_TYPE_STRING)) {
      string key = duk_to_string(ctx, -2);
      string value = string(duk_to_string(ctx, -1));
      attr.insert(pair<string,string>(key, value));
    }
    duk_pop_2(ctx);  /* pop key value  */
  }

  duk_pop_2(ctx); // popping enum object

  return attr;
}
