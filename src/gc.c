#include "gc.h"

static void mark(hiss_entry* v){
    if(v->marked) return;

    v->marked = HISS_TRUE;
}

static void mark_all(hiss_hashtable* t){
    unsigned int i;

    for(i = 0; i < t->size; i++)
        mark(t->table[i]);
}

static void sweep(hiss_hashtable* t){
    hiss_entry* unreached;
    hiss_entry** e = t->table;

    while(*e){
        if(!(*e)->marked){
            unreached = *e;

            *e = unreached->next;
            hiss_table_remove(t, unreached->key);
        }else{
            (*e)->marked = 0;
            e = &(*e)->next;
        }
    }
}

void gc(hiss_hashtable* t){
    mark_all(t);
    sweep(t);
}

