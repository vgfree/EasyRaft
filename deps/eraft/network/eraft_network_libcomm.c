#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "comm_api.h"

#include "eraft_confs.h"
#include "rbtree_cache.h"
#include "eraft_network.h"

typedef struct libcomm_eraft_connection
{
	char    *identity;
	char    host[IPV4_HOST_LEN];
	char    port[IPV4_PORT_LEN];

	/* tell if we need to connect or not */
	enum
	{
		CONNECTION_STATE_DISCONNECTED = 0,
		CONNECTION_STATE_CONNECTING,
		CONNECTION_STATE_CONNECTED,
	}       state;

	int     sfd;

	void    *network;
} libcomm_eraft_connection_t;

struct libcomm_eraft_network
{
	void                            *rbt_handle;	/*存放本端到远端的连接,只为发送数据*/

	int                             listen_port;

	struct comm_context             *commctx;

	ERAFT_NETWORK_ON_CONNECTED      on_connected_fcb;
	ERAFT_NETWORK_ON_ACCEPTED       on_accepted_fcb;
	ERAFT_NETWORK_ON_DISCONNECTED   on_disconnected_fcb;
	ERAFT_NETWORK_ON_TRANSMIT       on_transmit_fcb;
	void                            *usr;
};

static libcomm_eraft_connection_t *_new_connection(struct libcomm_eraft_network *network, char *host, char *port, int sfd)
{
	libcomm_eraft_connection_t *conn = calloc(1, sizeof(libcomm_eraft_connection_t));

	conn->network = network;

	snprintf(conn->host, sizeof(conn->host), "%s", host);
	snprintf(conn->port, sizeof(conn->port), "%s", port);

	conn->sfd = sfd;
	return conn;
}

static libcomm_eraft_connection_t *_find_connection(struct libcomm_eraft_network *network, char *host, char *port)
{
	char key[IPV4_HOST_LEN + IPV4_PORT_LEN] = { 0 };

	snprintf(key, sizeof(key), "%s:%s", host, port);

	libcomm_eraft_connection_t      *conn = NULL;
	int                             ret = RBTCacheGet(network->rbt_handle, key, strlen(key) + 1, &conn, sizeof(conn));

	if (ret != sizeof(conn)) {
		return NULL;
	}

	return conn;
}

static void client_event_fun(void *ctx, int socket, enum STEP_CODE step, void *usr)
{
	// struct comm_context *commctx = (struct comm_context *)ctx;

	switch (step)
	{
		case STEP_INIT:
		{
			printf("client here is connect : %d\n", socket);
			struct libcomm_eraft_network *network = usr;
			assert(network);

			struct connfd_info              *connfd = ((struct comm_context *)ctx)->commevts.connfd[socket];
			libcomm_eraft_connection_t      *conn = _find_connection(network, connfd->commtcp.peerhost, connfd->commtcp.peerport);
			conn->state = CONNECTION_STATE_CONNECTED;

			if (network->on_connected_fcb) {
				network->on_connected_fcb(conn, network->usr);
			}
		}
		break;

		case STEP_ERRO:
			printf("client here is error : %d\n", socket);
			{
				struct libcomm_eraft_network *network = usr;
				assert(network);

				struct connfd_info              *connfd = ((struct comm_context *)ctx)->commevts.connfd[socket];
				libcomm_eraft_connection_t      *conn = _find_connection(network, connfd->commtcp.peerhost, connfd->commtcp.peerport);
				conn->state = CONNECTION_STATE_DISCONNECTED;

				if (network->on_disconnected_fcb) {
					network->on_disconnected_fcb(conn, network->usr);
				}
			}
			break;

		case STEP_WAIT:
			printf("client here is wait : %d\n", socket);
			break;

		case STEP_STOP:
			printf("client here is close : %d\n", socket);
			break;

		default:
			printf("unknow!\n");
			break;
	}
}

static void server_event_fun(void *ctx, int socket, enum STEP_CODE step, void *usr)
{
	// struct comm_context *commctx = (struct comm_context *)ctx;

	switch (step)
	{
		case STEP_SWAP:
			printf("server here is switch : %d\n", socket);
			break;

		case STEP_INIT:
			printf("server here is accept : %d\n", socket);
			struct libcomm_eraft_network *network = usr;

			struct connfd_info              *connfd = ((struct comm_context *)ctx)->commevts.connfd[socket];
			libcomm_eraft_connection_t      *conn = _new_connection(network, connfd->commtcp.peerhost, connfd->commtcp.peerport, socket);

			if (network->on_accepted_fcb) {
				network->on_accepted_fcb(conn, network->usr);
			}

			break;

		case STEP_ERRO:
			printf("server here is error : %d\n", socket);
			// commapi_close(commctx, socket);
			break;

		case STEP_WAIT:
			printf("server here is wait : %d\n", socket);
			break;

		case STEP_STOP:
			printf("server here is close : %d\n", socket);
			break;

		default:
			printf("unknow!\n");
			break;
	}
}

bool libcomm_eraft_network_usable_connection(void *handle, eraft_connection_t *conn)
{
	return (CONNECTION_STATE_CONNECTED == ((libcomm_eraft_connection_t *)conn)->state) ? true : false;
}

static libcomm_eraft_connection_t *_connect_by_create(struct libcomm_eraft_network *network, char *host, char *port)
{
	// conn->state = CONNECTION_STATE_CONNECTING;
	// printf("Connecting to %s:%s\n", conn->host, conn->port);

	struct comm_cbinfo cbinfo = { 0 };

	cbinfo.monitor = true;
	cbinfo.fcb = client_event_fun;
	cbinfo.usr = network;

	int fd = commapi_socket(network->commctx, host, port, &cbinfo, COMM_CONNECT);

	libcomm_eraft_connection_t *conn = _new_connection(network, host, port, fd);

	return conn;
}

eraft_connection_t *libcomm_eraft_network_find_connection(void *handle, char *host, char *port)
{
	struct libcomm_eraft_network *network = handle;

	libcomm_eraft_connection_t *conn = _find_connection(network, host, port);

	if (!conn) {
		conn = _connect_by_create(network, host, port);

		char key[IPV4_HOST_LEN + IPV4_PORT_LEN] = { 0 };
		snprintf(key, sizeof(key), "%s:%s", host, port);

		int ret = RBTCacheSet(network->rbt_handle, key, strlen(key) + 1, &conn, sizeof(conn));
		assert(ret == sizeof(conn));
	}

	return (eraft_connection_t *)conn;
}

static void __peer_msg_send(struct comm_context *commctx, int sfd, struct iovec buf[], int num)
{
	struct comm_sds *sds = commsds_make(NULL, 4 << 10);

	for (int i = 0; i < num; i++) {
		commsds_push_tail(sds, buf[i].iov_base, buf[i].iov_len);
	}

	size_t  all = commsds_length(sds);
	char    *str = malloc(all);
	commsds_pull_tail(sds, str, all);

	struct comm_message message = { 0 };
	commmsg_make(&message, 4 << 10);
	commmsg_sets(&message, sfd, 0, EMPTY_METHOD);
	commmsg_frame_set(&message, 0, all, str);
	message.package.frames_of_package[0] = message.package.frames;
	message.package.packages = 1;

	int err = commapi_send(commctx, &message);

	if (err) {
		printf("ERR: send failed!\n");
	}

	commmsg_free(&message);

	free(str);
	commsds_free(sds);
}

void libcomm_eraft_network_transmit_connection(void *handle, eraft_connection_t *conn, struct iovec buf[], int num)
{
	struct libcomm_eraft_network    *network = handle;
	libcomm_eraft_connection_t      *_conn = (libcomm_eraft_connection_t *)conn;

	__peer_msg_send(network->commctx, _conn->sfd, buf, num);
}

void libcomm_eraft_network_info_connection(void *handle, eraft_connection_t *conn, char host[IPV4_HOST_LEN], char port[IPV4_PORT_LEN])
{
	libcomm_eraft_connection_t *_conn = (libcomm_eraft_connection_t *)conn;

	snprintf(host, IPV4_HOST_LEN, "%s", _conn->host);
	snprintf(port, IPV4_PORT_LEN, "%s", _conn->port);
}

bool _recv_filter(void *ctx, int sfd, void *msg, void *drx)
{
	struct comm_message *message = msg;

	/*过滤非对话包*/
	if (message->ptype != EMPTY_METHOD) {
		return false;
	}

	struct libcomm_eraft_network *network = drx;
	assert(network);

	libcomm_eraft_connection_t *conn = NULL;
	/*this peerport not peer listen port*/
	// struct connfd_info *connfd = ((struct comm_context *)ctx)->commevts.connfd[sfd];
	// conn = _find_connection(network, connfd->commtcp.peerhost, connfd->commtcp.peerport);

	int     frmsize = 0;
	char    *frmbuff = commmsg_frame_get(message, 0, &frmsize);

	if (network->on_transmit_fcb) {
		network->on_transmit_fcb(conn, frmbuff, frmsize, network->usr);
	}

	return true;
}

int eraft_network_init_libcomm(struct eraft_network *network, int listen_port,
	ERAFT_NETWORK_ON_CONNECTED on_connected_fcb,
	ERAFT_NETWORK_ON_ACCEPTED on_accepted_fcb,
	ERAFT_NETWORK_ON_DISCONNECTED on_disconnected_fcb,
	ERAFT_NETWORK_ON_TRANSMIT on_transmit_fcb,
	void *usr)
{
	struct libcomm_eraft_network *_network = calloc(1, sizeof(struct libcomm_eraft_network));

	network->handle = _network;
	network->api.find_connection = libcomm_eraft_network_find_connection;
	network->api.usable_connection = libcomm_eraft_network_usable_connection;
	network->api.transmit_connection = libcomm_eraft_network_transmit_connection;
	network->api.info_connection = libcomm_eraft_network_info_connection;

	_network->on_connected_fcb = on_connected_fcb;
	_network->on_accepted_fcb = on_accepted_fcb;
	_network->on_disconnected_fcb = on_disconnected_fcb;
	_network->on_transmit_fcb = on_transmit_fcb;
	_network->usr = usr;

	RBTCacheCreate(&_network->rbt_handle);

	_network->listen_port = listen_port;

	struct comm_context *commctx = commapi_ctx_create();
	assert(commctx);
	_network->commctx = commctx;

	char port[IPV4_PORT_LEN] = { 0 };
	sprintf(port, "%d", listen_port);

	struct comm_cbinfo cbinfo = { 0 };
	cbinfo.monitor = true;
	cbinfo.fcb = server_event_fun;
	cbinfo.usr = _network;
	cbinfo.frx = _recv_filter;
	cbinfo.drx = _network;

	int fd = commapi_socket(commctx, "0.0.0.0", port, &cbinfo, COMM_BIND);
	assert(-1 != fd);

	return 0;
}

int eraft_network_free_libcomm(struct eraft_network *network)
{
	struct libcomm_eraft_network *_network = (struct libcomm_eraft_network *)network->handle;

	RBTCacheDestory(&_network->rbt_handle);
	commapi_ctx_destroy(_network->commctx);
	free(_network);
	return 0;
}

