#pragma once

#include "eraft_network.h"

typedef int (*ERAFT_NETWORK_IMPL_INIT)(struct eraft_network *network, int listen_port,
	ERAFT_NETWORK_ON_CONNECTED on_connected_fcb,
	ERAFT_NETWORK_ON_ACCEPTED on_accepted_fcb,
	ERAFT_NETWORK_ON_DISCONNECTED on_disconnected_fcb,
	ERAFT_NETWORK_ON_TRANSMIT on_transmit_fcb,
	void *usr);

typedef int (*ERAFT_NETWORK_IMPL_FREE)(struct eraft_network *network);

enum ERAFT_NETWORK_TYPE
{
        ERAFT_NETWORK_TYPE_LIBUV = 0,
        ERAFT_NETWORK_TYPE_LIBCOMM = 1,
};


ERAFT_NETWORK_IMPL_INIT eraft_network_mapping_init(enum ERAFT_NETWORK_TYPE type);

ERAFT_NETWORK_IMPL_FREE eraft_network_mapping_free(enum ERAFT_NETWORK_TYPE type);


int eraft_network_init_libuv(struct eraft_network *network, int listen_port,
	ERAFT_NETWORK_ON_CONNECTED on_connected_fcb,
	ERAFT_NETWORK_ON_ACCEPTED on_accepted_fcb,
	ERAFT_NETWORK_ON_DISCONNECTED on_disconnected_fcb,
	ERAFT_NETWORK_ON_TRANSMIT on_transmit_fcb,
	void *usr);

int eraft_network_free_libuv(struct eraft_network *network);

int eraft_network_init_libcomm(struct eraft_network *network, int listen_port,
	ERAFT_NETWORK_ON_CONNECTED on_connected_fcb,
	ERAFT_NETWORK_ON_ACCEPTED on_accepted_fcb,
	ERAFT_NETWORK_ON_DISCONNECTED on_disconnected_fcb,
	ERAFT_NETWORK_ON_TRANSMIT on_transmit_fcb,
	void *usr);

int eraft_network_free_libcomm(struct eraft_network *network);

