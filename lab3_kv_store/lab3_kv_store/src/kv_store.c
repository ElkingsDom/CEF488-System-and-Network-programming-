#include <string.h>
#include <stdio.h>
#include "../include/kv_store.h"

#define MAX_ITEMS 1024

struct kv_entry {
    char key[MAX_KEY_SIZE];
    char value[MAX_VAL_SIZE];
    int active;
};

static struct kv_entry db[MAX_ITEMS];

int kv_store_init(void) {
    memset(db, 0, sizeof(db));
    return 0;
}

int kv_store_set(const char *key, const char *value) {
    if (!key || !value || strlen(key) == 0 || strlen(key) >= MAX_KEY_SIZE || strlen(value) >= MAX_VAL_SIZE) {
        return -1;
    }

    /* Check if key already exists to update it */
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (db[i].active && strcmp(db[i].key, key) == 0) {
            strncpy(db[i].value, value, MAX_VAL_SIZE - 1);
            db[i].value[MAX_VAL_SIZE - 1] = '\0';
            return 0;
        }
    }

    /* Find an empty slot for insert */
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (!db[i].active) {
            strncpy(db[i].key, key, MAX_KEY_SIZE - 1);
            db[i].key[MAX_KEY_SIZE - 1] = '\0';
            strncpy(db[i].value, value, MAX_VAL_SIZE - 1);
            db[i].value[MAX_VAL_SIZE - 1] = '\0';
            db[i].active = 1;
            return 0;
        }
    }

    return -1; 
}

int kv_store_get(const char *key, char *value_out, int max_len) {
    if (!key || !value_out) {
        return -1;
    }

    for (int i = 0; i < MAX_ITEMS; i++) {
        if (db[i].active && strcmp(db[i].key, key) == 0) {
            strncpy(value_out, db[i].value, max_len - 1);
            value_out[max_len - 1] = '\0';
            return 0;
        }
    }

    return -1;
}

int kv_store_del(const char *key) {
    if (!key) {
        return -1;
    }

    for (int i = 0; i < MAX_ITEMS; i++) {
        if (db[i].active && strcmp(db[i].key, key) == 0) {
            db[i].active = 0;
            memset(db[i].key, 0, MAX_KEY_SIZE);
            memset(db[i].value, 0, MAX_VAL_SIZE);
            return 0;
        }
    }

    return -1;
}

void kv_store_destroy(void) {
    memset(db, 0, sizeof(db));
}
