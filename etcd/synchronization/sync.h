#ifndef __SYNC_H_
#define __SYNC_H_

#include <stdint.h>
#include <stdlib.h>

typedef struct sync_t sync_t;

int sync_create(sync_t **S, uint32_t local_id, uint32_t universe_size, char **error);

int sync_configure(sync_t *sync_t, const char *key, const char *value, char **error);

int sync_initialize(sync_t *S, char **error);

int sync_set_state(sync_t *S, int state, char **error);

int sync_get_state(sync_t *S, int remote_id, char **error);

int sync_wait_for_global_state(sync_t *S, uint32_t desired_state, char **error);

int sync_destroy(sync_t *S, char **error);

void sync_cancel(sync_t *S);

#endif // __SYNC_H_
