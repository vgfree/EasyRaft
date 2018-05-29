#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "rbtree_cache.h"
#include "eraft_network.h"
#include "eraft_confs.h"

bool eraft_network_usable_connection(eraft_connection_t *conn)
{
	return (CONNECTION_STATE_CONNECTED == conn->state) ? true : false;
}

#ifdef JUST_FOR_TEST
static void __peer_alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf);

static void __on_connection_transmit_by_peer(uv_stream_t *tcp, ssize_t nread, const uv_buf_t *buf);

#endif
/*=================================发出的连接============================================*/

/** Our connection attempt to raft peer has succeeded */
static void __on_connection_connected_to_peer(uv_connect_t *req, const int status)
{
	switch (status)
	{
		case 0:
			break;

		case -ECONNREFUSED:
			// TODO: close uv_connect_t
			return;

		default:
			uv_fatal(status);
			return;
	}

	eraft_connection_t *conn = req->data;
	conn->state = CONNECTION_STATE_CONNECTED;

	int     nlen = sizeof(conn->addr);
	int     e = uv_tcp_getpeername((uv_tcp_t *)req->handle, (struct sockaddr *)&conn->addr, &nlen);

	if (0 != e) {
		uv_fatal(e);
	}

	struct eraft_network *network = conn->network;
	assert(network);

	if (network->on_connected_fcb) {
		network->on_connected_fcb(conn, network->usr);
	}

#ifdef JUST_FOR_TEST
	e = uv_read_start((uv_stream_t *)&conn->tcp, __peer_alloc_cb, __on_connection_transmit_by_peer);

	if (0 != e) {
		uv_fatal(e);
	}
#endif
}

/** Connect to raft peer */
static void __connect_to_peer(eraft_connection_t *conn)
{
	conn->state = CONNECTION_STATE_CONNECTING;
	printf("Connecting to %s:%s\n", conn->host, conn->port);

	uv_connect_t *c = calloc(1, sizeof(uv_connect_t));
	c->data = conn;
	int e = uv_tcp_connect(c, (uv_tcp_t *)conn->stream, (struct sockaddr *)&conn->addr,
			__on_connection_connected_to_peer);

	if (0 != e) {
		uv_fatal(e);
	}
}

static eraft_connection_t *_new_connection(struct eraft_network *network, uv_loop_t *loop)
{
	eraft_connection_t *conn = calloc(1, sizeof(eraft_connection_t));

	INIT_LIST_NODE(&conn->node);
	commcache_init(&conn->cache);
	conn->loop = loop;
	conn->network = network;

	uv_tcp_t *tcp = &conn->tcp;
	tcp->data = conn;
	int e = uv_tcp_init(conn->loop, tcp);

	if (0 != e) {
		uv_fatal(e);
	}

	conn->stream = (uv_stream_t *)tcp;

	return conn;
}

static eraft_connection_t *_connect_by_create(struct eraft_network *network, uv_loop_t *loop, char *host, char *port)
{
	eraft_connection_t *conn = _new_connection(network, loop);

	snprintf(conn->host, sizeof(conn->host), "%s", host);
	snprintf(conn->port, sizeof(conn->port), "%s", port);

	int e = uv_ip4_addr(host, atoi(port), &conn->addr);

	if (0 != e) {
		uv_fatal(e);
	}

	__connect_to_peer(conn);

	return conn;
}

/* Initiate connection if we are disconnected */
static int _connect_if_needed(eraft_connection_t *conn)
{
	if (CONNECTION_STATE_CONNECTED == conn->state) {
		return 0;
	}

	if (CONNECTION_STATE_DISCONNECTED == conn->state) {
		__connect_to_peer(conn);
	}

	return -1;
}

eraft_connection_t *eraft_network_find_connection(struct eraft_network *network, uv_loop_t *loop, char *host, char *port)
{
	char key[IPV4_HOST_LEN + IPV4_PORT_LEN] = { 0 };

	snprintf(key, sizeof(key), "%s:%s", host, port);

	eraft_connection_t      *conn = NULL;
	int                     ret = RBTCacheGet(network->rbt_handle, key, strlen(key) + 1, &conn, sizeof(conn));

	if (ret == sizeof(conn)) {
		_connect_if_needed(conn);
	} else {
		conn = _connect_by_create(network, loop, host, port);

		ret = RBTCacheSet(network->rbt_handle, key, strlen(key) + 1, &conn, sizeof(conn));
		assert(ret == sizeof(conn));
	}

	return conn;
}

/*=================================收到的连接============================================*/

static void __peer_alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
	buf->len = size;
	buf->base = malloc(size);
}

/** Read raft traffic using binary protocol */
static void __on_connection_transmit_by_peer(uv_stream_t *tcp, ssize_t nread, const uv_buf_t *buf)
{
	eraft_connection_t      *conn = tcp->data;
	struct eraft_network    *network = conn->network;

	if (nread < 0) {
#if 1
		list_del(&conn->node);
		conn->state = CONNECTION_STATE_DISCONNECTED;

		if (network->on_disconnected_fcb) {
			network->on_disconnected_fcb(conn, network->usr);
		}

		commcache_free(&conn->cache);
		// TODO: free
		return;
#else
		switch (nread)
		{
			case UV__ECONNRESET:
			case UV__EOF:
				conn->state = CONNECTION_STATE_DISCONNECTED;
				return;

			default:
				uv_fatal(nread);
		}
#endif
	}

	if (0 <= nread) {
		assert(conn);

		if (network->on_transmit_fcb) {
			network->on_transmit_fcb(conn, buf, nread, network->usr);
		}
	}
}

/** Raft peer has connected to us.
* Add them to our list of nodes */
static void __on_connection_accepted_by_peer(uv_stream_t *listener, const int status)
{
	if (0 != status) {
		uv_fatal(status);
	}

	uv_tcp_t                *tcp = (uv_tcp_t *)listener;
	struct eraft_network    *network = tcp->data;

	eraft_connection_t *conn = _new_connection(network, listener->loop);

	int e = uv_accept(listener, (uv_stream_t *)&conn->tcp);

	if (0 != e) {
		uv_fatal(e);
	}

	int namelen = sizeof(conn->addr);
	e = uv_tcp_getpeername(&conn->tcp, (struct sockaddr *)&conn->addr, &namelen);

	if (0 != e) {
		uv_fatal(e);
	}

	list_add_tail(&conn->node, &network->list_handle);

	if (network->on_accepted_fcb) {
		network->on_accepted_fcb(conn, network->usr);
	}

	e = uv_read_start((uv_stream_t *)&conn->tcp, __peer_alloc_cb, __on_connection_transmit_by_peer);

	if (0 != e) {
		uv_fatal(e);
	}
}

#define MAX_PEER_CONNECTIONS 128

int eraft_network_init(struct eraft_network *network, uv_loop_t *loop, int listen_port,
	ERAFT_NETWORK_ON_CONNECTED on_connected_fcb,
	ERAFT_NETWORK_ON_ACCEPTED on_accepted_fcb,
	ERAFT_NETWORK_ON_DISCONNECTED on_disconnected_fcb,
	ERAFT_NETWORK_ON_TRANSMIT on_transmit_fcb,
	void *usr)
{
	network->on_connected_fcb = on_connected_fcb;
	network->on_accepted_fcb = on_accepted_fcb;
	network->on_disconnected_fcb = on_disconnected_fcb;
	network->on_transmit_fcb = on_transmit_fcb;
	network->usr = usr;

	INIT_LIST_HEAD(&network->list_handle);

	uv_tcp_t *tcp = &network->listen_tcp;
	tcp->data = network;
	network->listen_stream = (uv_stream_t *)tcp;

	network->listen_port = listen_port;
	uv_bind_listen_socket(tcp, "0.0.0.0", listen_port, loop);
	int e = uv_listen(network->listen_stream, MAX_PEER_CONNECTIONS, __on_connection_accepted_by_peer);

	if (0 != e) {
		uv_fatal(e);
	}

	network->loop = loop;

	return RBTCacheCreate(&network->rbt_handle);
}

int eraft_network_free(struct eraft_network *network)
{
	return RBTCacheDestory(&network->rbt_handle);
}

