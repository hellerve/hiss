#ifndef HISS_TYPE_TABLE
#define HISS_TYPE_TABLE

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "type_management.h"

#include "../types/tables.h"

hiss_type_table* hiss_type_new();
hiss_type_table* hiss_type_copy(hiss_type_table* e);
void hiss_type_insert(hiss_type_table* hasht, const char* key, const struct hiss_val* value);
const struct hiss_val* hiss_type_get(hiss_type_table* hasht, const char* key);
void  hiss_type_remove(hiss_type_table* hasht, const char* key);
void hiss_type_delete(hiss_type_table* hasht);


#endif
