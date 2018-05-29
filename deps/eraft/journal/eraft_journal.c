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


#include "eraft_journal.h"
#include "eraft_journal_ext.h"

#include "eraft_confs.h"
#include "eraft_utils.h"
void eraft_journal_init(struct eraft_journal *store, int acceptor_id, char *dbpath, uint64_t dbsize, int type)
{
	store->type = type;

	ERAFT_JOURNAL_IMPL_INIT finit = eraft_journal_mapping_init(type);
	finit(store, acceptor_id, dbpath, dbsize);
}

void eraft_journal_free(struct eraft_journal *store)
{
	ERAFT_JOURNAL_IMPL_FREE ffree = eraft_journal_mapping_free(store->type);
	ffree(store);
}


int eraft_journal_open(struct eraft_journal *store)
{
       return store->api.open(store->handle);
}

void eraft_journal_close(struct eraft_journal *store)
{
       store->api.close(store->handle);
}


void *eraft_journal_tx_begin(struct eraft_journal *store)
{
	return store->api.tx_begin(store->handle);
}

int eraft_journal_tx_commit(struct eraft_journal *store, void *txn)
{
	return store->api.tx_commit(store->handle, txn);
}

void eraft_journal_tx_abort(struct eraft_journal *store, void *txn)
{
	store->api.tx_abort(store->handle, txn);
}

int eraft_journal_get_record(struct eraft_journal *store, void *txn, iid_t iid, struct eraft_entry *eentry)
{
	return store->api.get(store->handle, txn, iid, eentry);
}

int eraft_journal_set_record(struct eraft_journal *store, void *txn, iid_t iid, struct eraft_entry *eentry)
{
	return store->api.set(store->handle, txn, iid, eentry);
}

#if 0
int eraft_journal_trim(struct eraft_journal *store, void *txn, iid_t iid)
{
	return store->api.trim(store->handle, txn, iid);
}

iid_t eraft_journal_get_trim_instance(struct eraft_journal *store)
{
	return store->api.get_trim_instance(store->handle);
}
#endif

int eraft_journal_set_state(struct eraft_journal *store, char *key, size_t klen, char *val, size_t vlen)
{
	return store->api.set_state(store->handle, key, klen, val, vlen);
}

int eraft_journal_get_state(struct eraft_journal *store, char *key, size_t klen, char *val, size_t vlen)
{
	return store->api.get_state(store->handle, key, klen, val, vlen);
}
