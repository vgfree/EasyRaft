/*
 * Copyright (c) 2013-2015, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <assert.h>
#include <stdlib.h>

#include "eraft_network.h"
#include "eraft_network_ext.h"

#include "eraft_confs.h"
#include "eraft_utils.h"

int eraft_network_init(struct eraft_network *network, int type, int listen_port,
	ERAFT_NETWORK_ON_CONNECTED on_connected_fcb,
	ERAFT_NETWORK_ON_ACCEPTED on_accepted_fcb,
	ERAFT_NETWORK_ON_DISCONNECTED on_disconnected_fcb,
	ERAFT_NETWORK_ON_TRANSMIT on_transmit_fcb,
	void *usr)
{
	network->type = type;

	ERAFT_NETWORK_IMPL_INIT finit = eraft_network_mapping_init(type);
	return finit(network, listen_port, on_connected_fcb, on_accepted_fcb, on_disconnected_fcb, on_transmit_fcb, usr);
}

int eraft_network_free(struct eraft_network *network)
{
	ERAFT_NETWORK_IMPL_FREE ffree = eraft_network_mapping_free(network->type);

	return ffree(network);
}

eraft_connection_t *eraft_network_find_connection(struct eraft_network *network, char *host, char *port)
{
	return network->api.find_connection(network->handle, host, port);
}

bool eraft_network_usable_connection(struct eraft_network *network, eraft_connection_t *conn)
{
	return network->api.usable_connection(network->handle, conn);
}

void eraft_network_transmit_connection(struct eraft_network *network, eraft_connection_t *conn, struct iovec buf[], int num)
{
	return network->api.transmit_connection(network->handle, conn, buf, num);
}

void eraft_network_info_connection(struct eraft_network *network, eraft_connection_t *conn, char host[IPV4_HOST_LEN], char port[IPV4_PORT_LEN])
{
	network->api.info_connection(network->handle, conn, host, port);
}

