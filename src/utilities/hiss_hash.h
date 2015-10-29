#ifndef HISS_HASH
#define HISS_HASH

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "type_management.h"

#include "../core/gc.h"
#include "../types/tables.h"
#include "../types/types.h"

hiss_hashtable* hiss_table_new();
hiss_hashtable* hiss_table_copy(hiss_hashtable* e);
const hiss_val* hiss_table_insert(hiss_hashtable* hasht, const char* key, const hiss_val* value);
const hiss_val* hiss_table_get(hiss_hashtable* hasht, const char* key);
const hiss_val* hiss_table_remove(hiss_hashtable* hasht, const char* key);
void hiss_table_delete(hiss_hashtable* hasht);

#endif
