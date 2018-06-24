#pragma once

#include <string.h>
#include "list.h"

struct eraft_dotask;
typedef void (*ERAFT_DOTASK_FCB)(struct eraft_dotask *task, void *usr);

struct eraft_dotask
{
	struct list_node        node;
	int    type;
	bool			merge;
	char                    *identity;

	ERAFT_DOTASK_FCB	_fcb;
	void                    *_usr;
	char                    object[0];
};

void eraft_dotask_init(struct eraft_dotask *task, int type, bool merge, char *identity, ERAFT_DOTASK_FCB _fcb, void *_usr);

void eraft_dotask_free(struct eraft_dotask *task);
