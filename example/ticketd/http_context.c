#include "http_context.h"

#define ANYPORT                 65535
#define IPC_PIPE_NAME           "ticketd_ipc"
#define HTTP_WORKERS            4
#define MAX_HTTP_CONNECTIONS    128

/** Received an HTTP connection from client */
static void __on_http_connection(uv_stream_t *listener, const int status)
{
	if (0 != status) {
		uv_fatal(status);
	}

	uv_tcp_t *tcp = calloc(1, sizeof(*tcp));
	int e = uv_tcp_init(listener->loop, tcp);
	if (0 != e) {
		uv_fatal(e);
	}

	e = uv_accept(listener, (uv_stream_t *)tcp);
	if (0 != e) {
		uv_fatal(e);
	}

	struct http_context *hctx = ((uv_tcp_t *)listener)->data;

	struct timeval connected_at = *h2o_get_timestamp(&hctx->ctx, NULL, NULL);

	h2o_socket_t *sock = h2o_uv_socket_create((uv_stream_t *)tcp, (uv_close_cb)free);
	hctx->accept_ctx.ctx = &hctx->ctx;
	hctx->accept_ctx.hosts = hctx->cfg.hosts;

	h2o_http1_accept(&hctx->accept_ctx, sock, connected_at);
}

static void __http_worker_start(void *uv_tcp)
{
	uv_tcp_t *listener = uv_tcp;
	struct http_context *hctx = listener->data;

	h2o_context_init(&hctx->ctx, listener->loop, &hctx->cfg);

	int e = uv_listen((uv_stream_t *)listener, MAX_HTTP_CONNECTIONS, __on_http_connection);
	if (0 != e) {
		uv_fatal(e);
	}

	uv_run(listener->loop, UV_RUN_DEFAULT);
}

static void __start_http_socket(struct http_context *hctx, const char *host, int port, uv_tcp_t *listen, uv_multiplex_t *m)
{
	memset(&hctx->http_loop, 0, sizeof(uv_loop_t));
	int e = uv_loop_init(&hctx->http_loop);
	if (0 != e) {
		uv_fatal(e);
	}

	uv_bind_listen_socket(listen, host, port, &hctx->http_loop);
	uv_multiplex_init(m, listen, IPC_PIPE_NAME, HTTP_WORKERS, __http_worker_start);

	for (int i = 0; i < HTTP_WORKERS; i++) {
		uv_multiplex_worker_create(m, i, hctx);
	}

	uv_multiplex_dispatch(m);
}


struct http_context *http_context_create(int port, HTTP_CONTEXT_ON_REQ on_req)
{
	struct http_context *hctx = calloc(1, sizeof(*hctx));
	/* web server for clients */
	h2o_config_init(&hctx->cfg);
	h2o_hostconf_t  *hostconf = h2o_config_register_host(&hctx->cfg,
			h2o_iovec_init(H2O_STRLIT("default")),
			ANYPORT);

	/* HTTP route for receiving entries from clients */
	h2o_pathconf_t  *pathconf = h2o_config_register_path(hostconf, "/", 0);
	h2o_chunked_register(pathconf);

	/* Registration one request processing function */
	h2o_handler_t   *handler = h2o_create_handler(pathconf, sizeof(*handler));
	handler->on_req = on_req;

	/*启动http服务*/
	__start_http_socket(hctx, "0.0.0.0", port, &hctx->http_listen, &hctx->m);

	return hctx;
}
