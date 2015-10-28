#include "hiss_type_table.h"

#define INIT_SIZE 1024
#define GROWTH 2
#define MAX_LOAD 1

#define HASH_MUL 97

static hiss_type_table* internal_hiss_type_new(unsigned int size){
    hiss_type_table* hasht = NULL;
    unsigned int i;

    if(size < 1) return NULL;

    hasht = (hiss_type_table*) malloc(sizeof(hiss_type_table));

    assert(hasht != 0);

    hasht->size = size;
    hasht->table = (hiss_type_entry**) malloc(sizeof(hiss_type_entry* ) * hasht->size);

    for(i = 0; i < size; i++) hasht->table[i] = NULL;

    return hasht;
}

hiss_type_table* hiss_type_new(){
    return internal_hiss_type_new(INIT_SIZE);
}

hiss_type_table* hiss_type_copy(hiss_type_table* hasht){
    unsigned int i;
    hiss_type_table* new = internal_hiss_type_new(hasht->size);
    hiss_type_entry* e;

    for(i = 0; i < hasht->size; i++)
        for(e = hasht->table[i]; e != 0; e = e->next)
            hiss_type_insert(new, e->key, e->value);

    return new;
}

void hiss_type_delete(hiss_type_table* hasht){
    unsigned int i;
    hiss_type_entry* e = NULL;
    hiss_type_entry* next = NULL;

    if(!hasht) return;

    for(i = 0; i < hasht->size; i++){
        for(e = hasht->table[i]; e != 0; e = next){
            next = e->next;

            free((char*)e->key);
            free((struct hiss_val*)e->value);
            free(e);
        }
    }

    free(hasht->table);
    free(hasht);
}

static unsigned long hiss_hash(const char* key){
    unsigned const char* us;
    unsigned long h = 0;

    for(us = (const unsigned char*) key; *us; us++)
        h = (h * HASH_MUL) + *us;

    return h;
}

static void grow(hiss_type_table* hasht){
    hiss_type_table* tmp;
    hiss_type_table swap;

    tmp = hiss_type_copy(hasht);

    /* By God, you're ugly */
    swap = *hasht;
    *hasht = *tmp;
    *tmp = swap;

    hiss_type_delete(tmp);
}

void hiss_type_insert(hiss_type_table* hasht, const char* key, const struct hiss_val* value){
    hiss_type_entry* e;
    unsigned long h;

    assert(key);
    assert(value);

    e = (hiss_type_entry*) malloc(sizeof(hiss_type_entry));

    assert(e);

    e->key = key;
    e->value = value;

    h = hiss_hash(key) % hasht->size;

    e->next = hasht->table[h];
    hasht->table[h] = e;

    hasht->n++;

    if(hasht->n >= hasht->size * MAX_LOAD) grow(hasht);
}

const struct hiss_val* hiss_type_get(hiss_type_table* hasht, const char* key){
    hiss_type_entry* e;

    for(e = hasht->table[hiss_hash(key) % hasht->size]; e; e = e->next)
        if(e->key == key) return e->value;

    return hiss_err("Not found: %s", key);
}

void hiss_type_remove(hiss_type_table* hasht, const char* key){
    hiss_type_entry** prev;
    hiss_type_entry* e;

    for(prev = &(hasht->table[hiss_hash(key) % hasht->size]); *prev; prev = &((*prev)->next)){
        if((*prev)->key == key){
            e = *prev;
            *prev = e->next;

            free(e);
            hiss_val_del((hiss_val*)e->value);
            free(e);

            return;
        }
    }
}
