#include "gc.h"

static void mark(hiss_val* v){
    int i;

    if(v->marked) return;

    v->marked = HISS_TRUE;

    if(v->formals)
        mark(v->formals);

    if(v->body)
        mark(v->body);

    for(i = 0; i < v->count; i++)
        mark(v->cells[i]);
}

static void mark_all(hiss_env* e){
    int i;

    for(i = 0; i < e->count; i++)
        mark(e->vals[i]);
}

static void sweep(hiss_env* e){
    int i = 0;
    hiss_val* unreached;
    hiss_val** v = &e->vals[i++];

    while(*v){
        if(!(*v)->marked){
            unreached = *v;

            v = &e->vals[i++];
            free(unreached);
        }else{
            (*v)->marked = 0;
            v = &e->vals[i++];
        }
    }

    e->count--;
}

void gc(hiss_env* e){
    mark_all(e);
    sweep(e);

    e->max = e->count * 2;
}

