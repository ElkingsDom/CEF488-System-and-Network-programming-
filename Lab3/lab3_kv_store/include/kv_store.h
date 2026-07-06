#ifndef KV_STORE_H
#define KV_STORE_H

#include "kv_protocol.h"

/* Initialize the internal database */
int kv_store_init(void);

/* Insert or update a key-value pair */
int kv_store_set(const char *key, const char *value);

/* Retrieve a value by its key. Returns 0 on success, -1 if not found */
int kv_store_get(const char *key, char *value_out, int max_len);

/* Delete a key-value pair. Returns 0 on success, -1 if not found */
int kv_store_del(const char *key);

/* Free resources allocated for the database */
void kv_store_destroy(void);

#endif /* KV_STORE_H */
