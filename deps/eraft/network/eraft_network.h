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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "eraft_utils.h"
#include "eraft_confs.h"
#include "uv.h"

typedef void eraft_connection_t;

typedef void (*ERAFT_NETWORK_ON_CONNECTED)(eraft_connection_t *conn, void *usr);
typedef void (*ERAFT_NETWORK_ON_ACCEPTED)(eraft_connection_t *conn, void *usr);
typedef void (*ERAFT_NETWORK_ON_DISCONNECTED)(eraft_connection_t *conn, void *usr);
typedef int (*ERAFT_NETWORK_ON_TRANSMIT)(eraft_connection_t *conn, char *img, uint64_t sz, void *usr);


struct eraft_network
{
	int type;

	void    *handle;
	struct
	{
		eraft_connection_t *(*find_connection)(void *handle, char *host, char *port);
		bool (*usable_connection)(void *handle, eraft_connection_t *conn);
		void (*transmit_connection)(void *handle, eraft_connection_t *conn, uv_buf_t buf[], int num);
		void (*info_connection)(void *handle, eraft_connection_t *conn, char host[IPV4_HOST_LEN], char port[IPV4_PORT_LEN]);

	}       api;
};

int eraft_network_init(struct eraft_network *network, int type, int listen_port,
	ERAFT_NETWORK_ON_CONNECTED on_connected_fcb,
	ERAFT_NETWORK_ON_ACCEPTED on_accepted_fcb,
	ERAFT_NETWORK_ON_DISCONNECTED on_disconnected_fcb,
	ERAFT_NETWORK_ON_TRANSMIT on_transmit_fcb,
	void *usr);


int eraft_network_free(struct eraft_network *network);



eraft_connection_t *eraft_network_find_connection(struct eraft_network *network, char *host, char *port);

bool eraft_network_usable_connection(struct eraft_network *network, eraft_connection_t *conn);

void eraft_network_transmit_connection(struct eraft_network *network, eraft_connection_t *conn, uv_buf_t buf[], int num);

void eraft_network_info_connection(struct eraft_network *network, eraft_connection_t *conn, char host[IPV4_HOST_LEN], char port[IPV4_PORT_LEN]);

#ifdef __cplusplus
}
#endif

