static void mark_all(hiss_env* e){
    int i;

    for(i = 0; i < e->count; i++)
        mark(e->vals[i]);
}

static void mark(hiss_val* v){
    int i;

    if(v->marked) return;

    v->marked = HISS_TRUE;

    if(a->formals)
        mark(a->formals);

    if(a->body)
        mark(a->body);

    for(i = 0; i < a->num; i++)
        mark(a->cells[i]);
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
            (*object)->marked = 0;
            object = &(*object)->next;
        }
    }

    e->count--;
}

void gc(hiss_env* e){
    mark_all(e);
    sweep(e);

    e->max = e->num * 2;
}

