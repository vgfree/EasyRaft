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
#include "raft.h"
#include "eraft_utils.h"


typedef uint32_t iid_t;


struct eraft_entry
{
        uint32_t aid;
        iid_t iid;
	raft_entry_t entry;
};

static inline size_t eraft_entry_cubage(struct eraft_entry *eentry)
{
	return sizeof(*eentry) + eentry->entry.data.len;
}

static inline int eraft_entry_decode(struct eraft_entry *eentry, char *data, size_t size)
{
	if (size < sizeof(*eentry)) {
		return -1;
	}
	memcpy(eentry, data, sizeof(*eentry));
	eentry->entry.data.buf = NULL;
	if (eraft_entry_cubage(eentry) != size) {
		return -1;
	}
	eentry->entry.data.buf = malloc(eentry->entry.data.len);
	memcpy(eentry->entry.data.buf, data + sizeof(*eentry), eentry->entry.data.len);
	return 0;
}

static inline int eraft_journal_encode(struct eraft_entry *eentry, char *data, size_t size)
{
	if (eraft_entry_cubage(eentry) != size) {
		return -1;
	}
	memcpy(data, eentry, sizeof(*eentry));
	((struct eraft_entry *)data)->entry.data.buf = NULL;
	memcpy(data + sizeof(*eentry), eentry->entry.data.buf, eentry->entry.data.len);
	return 0;
}


struct eraft_journal
{
	int type;

	void    *handle;
	struct
	{
		/*for entry*/
		int     (*open) (void *handle);
		void    (*close) (void *handle);
		void    *(*tx_begin) (void *handle);
		int     (*tx_commit) (void *handle, void *txn);
		void    (*tx_abort) (void *handle, void *txn);
		int     (*get) (void *handle, void *txn, iid_t iid, struct eraft_entry *eentry);	/*返回成功设置的个数*/
		int     (*set) (void *handle, void *txn, iid_t iid, struct eraft_entry *eentry);	/*返回成功查询的个数*/

//		int     (*trim) (void *handle, iid_t iid);
//		iid_t   (*get_trim_instance) (void *handle);

		/*for state*/
		int	(*set_state) (void *handle, char *key, size_t klen, char *val, size_t vlen);
		int	(*get_state) (void *handle, char *key, size_t klen, char *val, size_t vlen);
	}       api;
};

void eraft_journal_init(struct eraft_journal *store, int acceptor_id, char *dbpath, uint64_t dbsize, int type);

void eraft_journal_free(struct eraft_journal *store);


/*===for entry===*/
int eraft_journal_open(struct eraft_journal *store);

void eraft_journal_close(struct eraft_journal *store);

void *eraft_journal_tx_begin(struct eraft_journal *store);

int eraft_journal_tx_commit(struct eraft_journal *store, void *txn);

void eraft_journal_tx_abort(struct eraft_journal *store, void *txn);

int eraft_journal_get_record(struct eraft_journal *store, void *txn, iid_t iid, struct eraft_entry *eentry);

int eraft_journal_set_record(struct eraft_journal *store, void *txn, iid_t iid, struct eraft_entry *eentry);

//int eraft_journal_trim(struct eraft_journal *store, iid_t iid);

//iid_t eraft_journal_get_trim_instance(struct eraft_journal *store);



/*===for state===*/
int eraft_journal_set_state(struct eraft_journal *store, char *key, size_t klen, char *val, size_t vlen);

int eraft_journal_get_state(struct eraft_journal *store, char *key, size_t klen, char *val, size_t vlen);

#ifdef __cplusplus
}
#endif

