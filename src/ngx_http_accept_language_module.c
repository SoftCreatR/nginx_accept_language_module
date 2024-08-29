#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
  ngx_array_t langs;
} ngx_http_accept_language_loc_conf_t;

static void *ngx_http_accept_language_create_conf(ngx_conf_t *cf);
static char *ngx_http_accept_language_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);

static char *ngx_http_accept_language(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_accept_language_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);

static ngx_command_t ngx_http_accept_language_commands[] = {
  {
    ngx_string("set_from_accept_language"),
    NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
    ngx_http_accept_language,
    NGX_HTTP_LOC_CONF_OFFSET,
    offsetof(ngx_http_accept_language_loc_conf_t, langs),
    NULL
  },
  ngx_null_command
};

// No need for any configuration callback
static ngx_http_module_t ngx_http_accept_language_module_ctx = {
  NULL, // preconfiguration
  NULL, // postconfiguration
  NULL, // create_main_conf
  NULL, // init_main_conf
  NULL, // create_srv_conf
  NULL, // merge_srv_conf
  ngx_http_accept_language_create_conf, // create_loc_conf
  ngx_http_accept_language_merge_conf // merge_loc_conf
};

ngx_module_t ngx_http_accept_language_module = {
  NGX_MODULE_V1,
  &ngx_http_accept_language_module_ctx,       /* module context */
  ngx_http_accept_language_commands,          /* module directives */
  NGX_HTTP_MODULE,                       /* module type */
  NULL,                                  /* init master */
  NULL,                                  /* init module */
  NULL,                                  /* init process */
  NULL,                                  /* init thread */
  NULL,                                  /* exit thread */
  NULL,                                  /* exit process */
  NULL,                                  /* exit master */
  NGX_MODULE_V1_PADDING
};

static void * ngx_http_accept_language_create_conf(ngx_conf_t *cf)
{
  ngx_http_accept_language_loc_conf_t *conf;

  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_accept_language_loc_conf_t));
  if (conf == NULL) {
    return NULL;
  }

  if (ngx_array_init(&conf->langs, cf->pool, 4, sizeof(ngx_str_t)) != NGX_OK) {
    return NULL;
  }

  return conf;
}

static char * ngx_http_accept_language_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
  ngx_str_t *elt;

  ngx_http_accept_language_loc_conf_t *prev = parent;
  ngx_http_accept_language_loc_conf_t *conf = child;

  ngx_str_t *prev_langs = prev->langs.elts;

  if (conf->langs.nelts == 0) {
    for (uint x = 0; x < prev->langs.nelts; x++) {
      elt = ngx_array_push(&conf->langs);
      if (elt == NULL) {
        return NGX_CONF_ERROR;
      }
      *elt = prev_langs[x];
    }
  }

  return NGX_CONF_OK;
}

static char * ngx_http_accept_language(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_uint_t i;
  ngx_str_t *value, *elt, name;
  ngx_http_variable_t *var;

  value = cf->args->elts;
  name = value[1];

  if (name.data[0] != '$') {
    ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "\"%V\" variable name should start with '$'", &name);
  } else {
    name.len--;
    name.data++;
  }

  var = ngx_http_add_variable(cf, &name, NGX_HTTP_VAR_CHANGEABLE);
  if (var == NULL) {
    return NGX_CONF_ERROR;
  }
  if (var->get_handler == NULL) {
    var->get_handler = ngx_http_accept_language_variable;
  }

  ngx_http_accept_language_loc_conf_t *alcf = conf;

  for (i = 2; i < cf->args->nelts; i++) {
    elt = ngx_array_push(&alcf->langs);
    if (elt == NULL) {
      return NGX_CONF_ERROR;
    }
    *elt = value[i];
  }

  return NGX_CONF_OK;
}

static ngx_int_t ngx_http_accept_language_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_accept_language_loc_conf_t *alcf;
    alcf = ngx_http_get_module_loc_conf(r, ngx_http_accept_language_module);

    if (alcf == NULL || alcf->langs.nelts == 0) {
        v->valid = 0;
        v->no_cacheable = 1;
        v->not_found = 1;
        return NGX_OK;
    }

    ngx_str_t *langs = alcf->langs.elts;

    ngx_uint_t i = 0;
    ngx_uint_t found = 0;
    u_char *start, *pos, *end;

    ngx_table_elt_t *accept_language_header = NULL;
    ngx_list_part_t *part = &r->headers_in.headers.part;
    ngx_table_elt_t *header = part->elts;

    for (i = 0; /* void */; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        if (ngx_strcasecmp(header[i].key.data, (u_char *)"Accept-Language") == 0) {
            accept_language_header = &header[i];
            break;
        }
    }

    if (accept_language_header != NULL) {
        start = accept_language_header->value.data;
        end = start + accept_language_header->value.len;

        while (start < end) {
            // Skip spaces
            while (start < end && *start == ' ') {
                start++;
            }

            pos = start;

            while (pos < end && *pos != ',' && *pos != ';') {
                pos++;
            }

            for (i = 0; i < alcf->langs.nelts; i++) {
                if ((ngx_uint_t)(pos - start) >= langs[i].len && ngx_strncasecmp(start, langs[i].data, langs[i].len) == 0) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                break;
            }

            i = 0; // Default to the first language in the list

            // Discard the quality value
            if (*pos == ';') {
                while (pos < end && *pos != ',') {
                    pos++;
                }
            }
            if (*pos == ',') {
                pos++;
            }

            start = pos;
        }
    }

    v->data = langs[i].data;
    v->len = langs[i].len;

    // Set all required params
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    return NGX_OK;
}


