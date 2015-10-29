#include "hiss_hash.h"
#include <limits.h>

#define INIT_SIZE 65536
#define GROWTH 2
#define MAX_LOAD 0.99

#define HASH_MUL 97

static hiss_hashtable* internal_hiss_table_new(unsigned int size){
    hiss_hashtable* hasht = NULL;
    unsigned int i;

    if(size < 1) return NULL;

    hasht = (hiss_hashtable*) malloc(sizeof(hiss_hashtable));

    assert(hasht != 0);

    hasht->n = 0;
    hasht->size = size;
    hasht->table = (hiss_entry**) malloc(sizeof(hiss_entry* ) * hasht->size);

    for(i = 0; i < size; i++) hasht->table[i] = NULL;

    return hasht;
}

hiss_hashtable* hiss_table_new(){
    return internal_hiss_table_new(INIT_SIZE);
}

hiss_hashtable* hiss_table_copy(hiss_hashtable* hasht){
    unsigned int i;
    hiss_hashtable* new = internal_hiss_table_new(hasht->size);
    hiss_entry* e;

    for(i = 0; i < hasht->size; i++)
        for(e = hasht->table[i]; e != 0; e = e->next)
            hiss_table_insert(new, e->key, e->value);

    return new;
}

void hiss_table_delete(hiss_hashtable* hasht){
    unsigned int i;
    hiss_entry* e = NULL;
    hiss_entry* next = NULL;

    if(!hasht) return;

    for(i = 0; i < hasht->size; i++){
        for(e = hasht->table[i]; e != 0; e = next){
            next = e->next;

            free((char*)e->key);
            free((hiss_val*)e->value);
            free(e);
        }
    }

    free(hasht->table);
    free(hasht);
}

static unsigned long hiss_hash(const char* key){
  unsigned long hashval = 0;
  unsigned int i = 0;

  assert(key != NULL);

  while(hashval < ULONG_MAX && i < strlen(key)) {
    hashval = hashval << 8;
    hashval += (unsigned) key[i];
    i++;
  }

  return hashval;
}

static void grow(hiss_hashtable* hasht){
    // Has to compute new hashes
    hiss_hashtable* tmp;
    hiss_hashtable swap;

    tmp = hiss_table_copy(hasht);

    /* By God, you're ugly*/
    swap = *hasht;
    *hasht = *tmp;
    *tmp = swap;

    hiss_table_delete(tmp);
}

const hiss_val* hiss_table_insert(hiss_hashtable* hasht, const char* key, const hiss_val* value){
    hiss_entry* e;
    unsigned long h;

    if(!hasht || !hasht->size || !key || !value) return hiss_err("Invalid call to insert: %s", key);

    h = hiss_hash(key) % hasht->size;
    
    for (e = hasht->table[h]; e; e = e->next) if (strcmp(key, e->key) == 0) break;

    if (e != NULL) return hiss_err("Already defined: %s", key);

    e = (hiss_entry*) malloc(sizeof(hiss_entry));

    assert(e);

    e->key = key;
    e->value = value;

    e->next = hasht->table[h];
    hasht->table[h] = e;

    hasht->n++;

    if(hasht->n >= hasht->size * MAX_LOAD){ 
        gc(hasht);
        grow(hasht);
    }

    return hiss_val_bool(HISS_TRUE);
}

const hiss_val* hiss_table_get(hiss_hashtable* hasht, const char* key){
    unsigned long h = hiss_hash(key) % hasht->size;
    const hiss_entry* e;
    
    if (hasht->table[h] != NULL) {
      for (e = hasht->table[h]; e; e = e->next)
        if (strcmp(key, e->key) == 0)
          return e->value;
    }
    return NULL;
}

const hiss_val* hiss_table_remove(hiss_hashtable* hasht, const char* key){
    unsigned long h = hiss_hash(key);
    hiss_entry* e, *prev = NULL;

    if (hasht->table[h] != NULL) { 
      for (e = hasht->table[h]; e; e = e->next) {
        if (strcmp(key, e->key) == 0) {
          if (prev != NULL) prev->next = e->next;
          else hasht->table[h] = e->next;
          free(e);
          return hiss_val_bool(HISS_TRUE);
        }
        e = prev;
      }
    }
    return hiss_err("Not found: %s", key);
}
