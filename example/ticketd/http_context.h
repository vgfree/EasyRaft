#pragma once

#include "h2o.h"
#include "h2o/http1.h"
#include "h2o_helpers.h"

#include "uv.h"
#include "uv_helpers.h"
#include "uv_multiplex.h"


struct http_context {
	h2o_globalconf_t        cfg;
	h2o_context_t           ctx;
	h2o_accept_ctx_t        accept_ctx;


	uv_multiplex_t  m;
	uv_tcp_t        http_listen;
	uv_loop_t               http_loop;
};

typedef int (*HTTP_CONTEXT_ON_REQ)(struct st_h2o_handler_t *self, h2o_req_t *req);

struct http_context *http_context_create(int port, HTTP_CONTEXT_ON_REQ on_req);
