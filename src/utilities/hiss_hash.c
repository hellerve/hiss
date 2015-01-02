#include "hiss_hash.h"

#define INIT_SIZE 1024
#define GROWTH 2
#define MAX_LOAD 1

#define HASH_MUL 97

static hiss_hashtable* internal_hiss_table_new(unsigned int size){
    hiss_hashtable* hasht = NULL;
    unsigned int i;

    if(size < 1) return NULL;

    hasht = (hiss_hashtable*) malloc(sizeof(hiss_hashtable));

    assert(hasht != 0);

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

    for(i = 0; i < hasht->size; i++){
        for(e = hasht->table[i]; e != 0; e = next){
            next = e->next;

            free((hiss_val*)e->key);
            free((hiss_val*)e->value);
            free(e);
        }
    }

    free(hasht->table);
    free(hasht);
}

static unsigned long hiss_hash(const hiss_val* key){
    unsigned const char* us;
    unsigned long h = 0;

    for(us = (unsigned const char*) key->sym; *us; us++)
        h = (h * HASH_MUL) + *us;

    return h;
}

static void grow(hiss_hashtable* hasht){
    hiss_hashtable* tmp;
    hiss_hashtable swap;

    tmp = hiss_table_copy(hasht);

    /* By God, you're ugly*/
    swap = *hasht;
    *hasht = *tmp;
    *tmp = swap;

    hiss_table_delete(tmp);
}

void hiss_table_insert(hiss_hashtable* hasht, const hiss_val* key, const hiss_val* value){
    hiss_entry* e;
    unsigned long h;

    assert(key);
    assert(value);

    e = (hiss_entry*) malloc(sizeof(hiss_entry));

    assert(e);

    e->key = key;
    e->value = value;

    h = hiss_hash(key) % hasht->size;

    e->next = hasht->table[h];
    hasht->table[h] = e;

    hasht->n++;

    if(hasht->n >= hasht->size * MAX_LOAD){ 
        gc(hasht);
        grow(hasht);
    }
}

const hiss_val* hiss_table_get(hiss_hashtable* hasht, const hiss_val* key){
    hiss_entry* e;

    for(e = hasht->table[hiss_hash(key) % hasht->size]; e; e = e->next)
        if(e->key == key) return e->value;

    return NULL;
}

void hiss_table_remove(hiss_hashtable* hasht, const hiss_val* key){
    hiss_entry** prev;
    hiss_entry* e;

    for(prev = &(hasht->table[hiss_hash(key) % hasht->size]); *prev; prev = &((*prev)->next)){
        if((*prev)->key == key){
            e = *prev;
            *prev = e->next;

            hiss_val_del((hiss_val*)e->key);
            hiss_val_del((hiss_val*)e->value);
            free(e);

            return;
        }
    }
}
