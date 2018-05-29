#pragma once
#include <stdint.h>
#include <stdlib.h>

void *etask_tree_make(void);

void etask_tree_free(void *tree);

int etask_tree_make_task(void *tree, void *key, size_t klen);

int etask_tree_await_task(void *tree, void *key, size_t klen, int efd, int msec);

void etask_tree_awake_task(void *tree, void *key, size_t klen);

