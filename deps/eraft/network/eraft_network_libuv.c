#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "uv.h"
#include "uv_helpers.h"
#include "uv_multiplex.h"

#include "list.h"
#include "eraft_confs.h"
#include "comm_cache.h"
#include "rbtree_cache.h"
#include "eraft_network.h"

typedef struct libuv_eraft_connection
{
	char                    *identity;
	char                    host[IPV4_HOST_LEN];
	char                    port[IPV4_PORT_LEN];

	struct list_node        node;
	/* peer's address */
	struct sockaddr_in      addr;

	struct comm_cache       cache;

	/* tell if we need to connect or not */
	enum
	{
		CONNECTION_STATE_DISCONNECTED = 0,
		CONNECTION_STATE_CONNECTING,
		CONNECTION_STATE_CONNECTED,
	}                       state;

	union
	{
		uv_tcp_t        tcp;
		uv_udp_t        udp;
	};
	uv_stream_t             *stream;

	uv_loop_t               *loop;

	void                    *network;
} libuv_eraft_connection_t;

struct libuv_eraft_network
{
	void                            *rbt_handle;	/*存放本端到远端的连接,只为发送数据*/
	pthread_t                       pid;

	int                             listen_port;
	union
	{
		uv_tcp_t        listen_tcp;
		uv_udp_t        listen_udp;
	};
	uv_stream_t                     *listen_stream;
	struct list_head                list_handle;

	uv_loop_t                       loop;

	ERAFT_NETWORK_ON_CONNECTED      on_connected_fcb;
	ERAFT_NETWORK_ON_ACCEPTED       on_accepted_fcb;
	ERAFT_NETWORK_ON_DISCONNECTED   on_disconnected_fcb;
	ERAFT_NETWORK_ON_TRANSMIT       on_transmit_fcb;
	void                            *usr;
};

bool libuv_eraft_network_usable_connection(void *handle, eraft_connection_t *conn)
{
	return (CONNECTION_STATE_CONNECTED == ((libuv_eraft_connection_t *)conn)->state) ? true : false;
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

	libuv_eraft_connection_t *conn = req->data;
	conn->state = CONNECTION_STATE_CONNECTED;

	int     nlen = sizeof(conn->addr);
	int     e = uv_tcp_getpeername((uv_tcp_t *)req->handle, (struct sockaddr *)&conn->addr, &nlen);

	if (0 != e) {
		uv_fatal(e);
	}

	struct libuv_eraft_network *network = conn->network;
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
static void __connect_to_peer(libuv_eraft_connection_t *conn)
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

static libuv_eraft_connection_t *_new_connection(struct libuv_eraft_network *network, uv_loop_t *loop)
{
	libuv_eraft_connection_t *conn = calloc(1, sizeof(libuv_eraft_connection_t));

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

static libuv_eraft_connection_t *_connect_by_create(struct libuv_eraft_network *network, uv_loop_t *loop, char *host, char *port)
{
	libuv_eraft_connection_t *conn = _new_connection(network, loop);

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
static int _connect_if_needed(libuv_eraft_connection_t *conn)
{
	if (CONNECTION_STATE_CONNECTED == conn->state) {
		return 0;
	}

	if (CONNECTION_STATE_DISCONNECTED == conn->state) {
		__connect_to_peer(conn);
	}

	return -1;
}

eraft_connection_t *libuv_eraft_network_find_connection(void *handle, char *host, char *port)
{
	struct libuv_eraft_network      *network = handle;
	char                            key[IPV4_HOST_LEN + IPV4_PORT_LEN] = { 0 };

	snprintf(key, sizeof(key), "%s:%s", host, port);

	libuv_eraft_connection_t        *conn = NULL;
	int                             ret = RBTCacheGet(network->rbt_handle, key, strlen(key) + 1, &conn, sizeof(conn));

	if (ret == sizeof(conn)) {
		_connect_if_needed(conn);
	} else {
		conn = _connect_by_create(network, &network->loop, host, port);

		ret = RBTCacheSet(network->rbt_handle, key, strlen(key) + 1, &conn, sizeof(conn));
		assert(ret == sizeof(conn));
	}

	return (eraft_connection_t *)conn;
}

/*=================================收到的连接============================================*/
static void dispose_transmit_by_peer(struct libuv_eraft_network *network, libuv_eraft_connection_t *conn, const uv_buf_t *buf, ssize_t nread, void *usr)
{
	bool ok = commcache_import(&conn->cache, buf->base, nread);

	assert(ok);

	do {
		size_t have = commcache_size(&conn->cache);

		if (have <= sizeof(uint64_t)) {
			break;
		}

		uint64_t all = 0;
		ok = commcache_export(&conn->cache, (char *)&all, sizeof(uint64_t));
		assert(ok);

		if (have < all) {
			ok = commcache_resume(&conn->cache, (char *)&all, sizeof(uint64_t));
			assert(ok);
			break;
		}

		uint64_t        len = all - sizeof(uint64_t);
		char            *msg = calloc(1, len);
		ok = commcache_export(&conn->cache, (char *)msg, len);
		assert(ok);

		if (network->on_transmit_fcb) {
			network->on_transmit_fcb(conn, msg, len, network->usr);
		}

		free(msg);
	} while (1);
}

static void __peer_alloc_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
	buf->len = size;
	buf->base = malloc(size);
}

/** Read raft traffic using binary protocol */
static void __on_connection_transmit_by_peer(uv_stream_t *tcp, ssize_t nread, const uv_buf_t *buf)
{
	libuv_eraft_connection_t        *conn = tcp->data;
	struct libuv_eraft_network      *network = conn->network;

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
#endif		/* if 1 */
	}

	if (0 <= nread) {
		assert(conn);

		conn->state = CONNECTION_STATE_CONNECTED;
		dispose_transmit_by_peer(network, conn, buf, nread, network->usr);
	}
}

/** Raft peer has connected to us.
* Add them to our list of nodes */
static void __on_connection_accepted_by_peer(uv_stream_t *listener, const int status)
{
	if (0 != status) {
		uv_fatal(status);
	}

	uv_tcp_t                        *tcp = (uv_tcp_t *)listener;
	struct libuv_eraft_network      *network = tcp->data;

	libuv_eraft_connection_t *conn = _new_connection(network, listener->loop);

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

static void __peer_msg_send(uv_stream_t *s, uv_buf_t buf[], int num)
{
	uint64_t        all = 0;
	uv_buf_t        tmps[num + 1];

	tmps[0].len = sizeof(uint64_t);
	tmps[0].base = (char *)&all;
	memcpy(&tmps[1], buf, sizeof(uv_buf_t) * num);

	for (int i = 0; i < (num + 1); i++) {
		all += tmps[i].len;
	}

#if 1
	for (int i = 0; i < (num + 1); i++) {
		while (tmps[i].len) {
			int e = uv_try_write(s, &tmps[i], 1);

			if (e < 0) {
				// uv_fatal(e);
			} else {
				tmps[i].base += e;
				tmps[i].len -= e;
			}
		}
	}
#else
	uv_stream_set_blocking(s, 1);

	for (int i = 0; i < (num + 1); i++) {
		size_t nwritten = 0;

		while (nwritten < tmps[i].len) {
			/* The stream is in blocking mode so uv_try_write() should always succeed
			 * with the exact number of bytes that we wanted written.
			 */
			int e = uv_try_write(s, &tmps[i], 1);

			if (e < 0) {
				uv_fatal(e);
			} else {
				assert(e == tmps[i].len);
				nwritten += e;
			}
		}
	}
	uv_stream_set_blocking(s, 0);
#endif		/* if 1 */
}

void libuv_eraft_network_transmit_connection(void *handle, eraft_connection_t *conn, struct iovec buf[], int num)
{
	libuv_eraft_connection_t        *_conn = (libuv_eraft_connection_t *)conn;
	uv_buf_t                        uv_buf[num];

	for (int i = 0; i < num; i++) {
		uv_buf[i].base = buf[i].iov_base;
		uv_buf[i].len = buf[i].iov_len;
	}

	__peer_msg_send(_conn->stream, uv_buf, num);
}

void libuv_eraft_network_info_connection(void *handle, eraft_connection_t *conn, char host[IPV4_HOST_LEN], char port[IPV4_PORT_LEN])
{
	libuv_eraft_connection_t *_conn = (libuv_eraft_connection_t *)conn;

	snprintf(host, IPV4_HOST_LEN, "%s", inet_ntoa(_conn->addr.sin_addr));
	snprintf(port, IPV4_PORT_LEN, "%d", _conn->addr.sin_port);
}

void *_network_start(void *arg)
{
	struct libuv_eraft_network *_network = (struct libuv_eraft_network *)arg;

	do {
		// TODO: add break;

		uv_run(&_network->loop, UV_RUN_ONCE);
	} while (1);
	return NULL;
}

int eraft_network_init_libuv(struct eraft_network *network, int listen_port,
	ERAFT_NETWORK_ON_CONNECTED on_connected_fcb,
	ERAFT_NETWORK_ON_ACCEPTED on_accepted_fcb,
	ERAFT_NETWORK_ON_DISCONNECTED on_disconnected_fcb,
	ERAFT_NETWORK_ON_TRANSMIT on_transmit_fcb,
	void *usr)
{
	struct libuv_eraft_network *_network = calloc(1, sizeof(struct libuv_eraft_network));

	network->handle = _network;
	network->api.find_connection = libuv_eraft_network_find_connection;
	network->api.usable_connection = libuv_eraft_network_usable_connection;
	network->api.transmit_connection = libuv_eraft_network_transmit_connection;
	network->api.info_connection = libuv_eraft_network_info_connection;

	_network->on_connected_fcb = on_connected_fcb;
	_network->on_accepted_fcb = on_accepted_fcb;
	_network->on_disconnected_fcb = on_disconnected_fcb;
	_network->on_transmit_fcb = on_transmit_fcb;
	_network->usr = usr;

	INIT_LIST_HEAD(&_network->list_handle);

	/*初始化事件loop*/
	uv_loop_t *loop = &_network->loop;
	memset(loop, 0, sizeof(uv_loop_t));
	int e = uv_loop_init(loop);

	if (0 != e) {
		uv_fatal(e);
	}

	uv_tcp_t *tcp = &_network->listen_tcp;
	tcp->data = _network;
	_network->listen_stream = (uv_stream_t *)tcp;

	_network->listen_port = listen_port;
	uv_bind_listen_socket(tcp, "0.0.0.0", listen_port, loop);
	e = uv_listen(_network->listen_stream, MAX_PEER_CONNECTIONS, __on_connection_accepted_by_peer);

	if (0 != e) {
		uv_fatal(e);
	}

	RBTCacheCreate(&_network->rbt_handle);

	assert(pthread_create(&_network->pid, NULL, &_network_start, _network) == 0);
	return 0;
}

int eraft_network_free_libuv(struct eraft_network *network)
{
	struct libuv_eraft_network *_network = (struct libuv_eraft_network *)network->handle;

	RBTCacheDestory(&_network->rbt_handle);
	free(_network);
	return 0;
}

