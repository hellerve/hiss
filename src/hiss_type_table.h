#ifndef HISS_TYPE_TABLE
#define HISS_TYPE_TABLE

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "types.h"

hiss_type_table* hiss_type_new();
hiss_type_table* hiss_type_copy(hiss_type_table* e);
void hiss_type_insert(hiss_type_table* hasht, const char* key, const hiss_val* value);
const hiss_val*  hiss_type_get(hiss_type_table* hasht, const char* key);
void  hiss_type_remove(hiss_type_table* hasht, const char* key);
void hiss_type_delete(hiss_type_table* hasht);


#endif
