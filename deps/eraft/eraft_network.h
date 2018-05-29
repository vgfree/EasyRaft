#pragma once

#include "uv.h"
#include "uv_helpers.h"
#include "uv_multiplex.h"

#include "list.h"
#include "eraft_confs.h"
#include "comm_cache.h"


typedef struct eraft_connection
{
	char                    *identity;
	char                    host[IPV4_HOST_LEN];
	char                    port[IPV4_PORT_LEN];

	struct list_node        node;
	/* peer's address */
	struct sockaddr_in      addr;

	struct comm_cache	cache;

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
} eraft_connection_t;

typedef void (*ERAFT_NETWORK_ON_CONNECTED)(eraft_connection_t *conn, void *usr);
typedef void (*ERAFT_NETWORK_ON_ACCEPTED)(eraft_connection_t *conn, void *usr);
typedef void (*ERAFT_NETWORK_ON_DISCONNECTED)(eraft_connection_t *conn, void *usr);
typedef void (*ERAFT_NETWORK_ON_TRANSMIT)(eraft_connection_t *conn, const uv_buf_t *buf, ssize_t nread, void *usr);

struct eraft_network
{
	void                            *rbt_handle;	/*存放本端到远端的连接,只为发送数据*/

	int                             listen_port;
	union
	{
		uv_tcp_t        listen_tcp;
		uv_udp_t        listen_udp;
	};
	uv_stream_t                     *listen_stream;
	struct list_head                list_handle;

	uv_loop_t                       *loop;

	ERAFT_NETWORK_ON_CONNECTED      on_connected_fcb;
	ERAFT_NETWORK_ON_ACCEPTED       on_accepted_fcb;
	ERAFT_NETWORK_ON_DISCONNECTED   on_disconnected_fcb;
	ERAFT_NETWORK_ON_TRANSMIT       on_transmit_fcb;
	void                            *usr;
};

int eraft_network_init(struct eraft_network *network, uv_loop_t *loop, int listen_port,
	ERAFT_NETWORK_ON_CONNECTED on_connected_fcb,
	ERAFT_NETWORK_ON_ACCEPTED on_accepted_fcb,
	ERAFT_NETWORK_ON_DISCONNECTED on_disconnected_fcb,
	ERAFT_NETWORK_ON_TRANSMIT on_transmit_fcb,
	void *usr);

int eraft_network_free(struct eraft_network *network);

eraft_connection_t *eraft_network_find_connection(struct eraft_network *network, uv_loop_t *loop, char *host, char *port);

bool eraft_network_usable_connection(eraft_connection_t *conn);

