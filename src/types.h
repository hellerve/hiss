#ifndef TYPES_H
#define TYPES_H

#include "util.h"

enum {HISS_ERR, HISS_NUM, HISS_SYM, HISS_SEXPR};

enum {HISS_ZERO_DIV, HISS_BAD_OP, HISS_BAD_NUM};

typedef struct hiss_val {
    unsigned short type;
    long num;
    char* err;
    char* sym;
    unsigned int count;
    struct hiss_val** cells;
} hiss_val;

hiss_val* hiss_val_num(long n){
    hiss_val* val = malloc(sizeof(hiss_val));
    val->type = HISS_NUM;
    val->num = n;
    return val;
}

hiss_val* hiss_val_sym(char* s){
    hiss_val* val = malloc(sizeof(hiss_val));
    val->type = HISS_SYM;
    val->sym = malloc(strlen(s) + 1);
    strcpy(val->sym, s);
    return val;
}

hiss_val* hiss_val_sexpr(){
    hiss_val* val = malloc(sizeof(hiss_val));
    val->type = HISS_SEXPR;
    val->count = 0;
    val->cells = NULL;
    return val;
}

hiss_val* hiss_err(char* m){
    hiss_val* val = malloc(sizeof(hiss_val));
    val->type = HISS_ERR;
    val->err = malloc(strlen(m) + 1);
    strcpy(val->sym, m);
    return val;
}

hiss_val* hiss_val_add(hiss_val* v, hiss_val* a){
    v->count++;
    v->cells = realloc(v->cells, sizeof(hiss_val*) * v->count);
    v->cells[v->count-1] = a;
    return v;
}

hiss_val* hiss_val_read_num(vpc_ast* t){
    long n = strtol(t->contents, NULL, 10);
    errno = 0;
    return errno != ERANGE ? hiss_val_num(n) : hiss_err((char *)"Invalid number");
}

hiss_val* hiss_val_read(vpc_ast* t){
    unsigned int i;
    hiss_val* v = NULL;
    if(strstr(t->tag, "number")) return hiss_val_read_num(t);
    if(strstr(t->tag, "symbol")) return hiss_val_sym(t->contents);

    if(strcmp(t->tag, ">") == 0 || strstr(t->tag, "sexpr")) v = hiss_val_sexpr();

    for(i = 0; i < t->children_num; i++){
        if(strcmp(t->children[i]->contents, "(") == 0) continue;
        else if(strcmp(t->children[i]->contents, ")") == 0) continue;
        else if(strcmp(t->children[i]->contents, "}") == 0) continue;
        else if(strcmp(t->children[i]->contents, "{") == 0) continue; 
        else if(strcmp(t->children[i]->tag,  "regex") == 0) continue;
        v = hiss_val_add(v, hiss_val_read(t->children[i]));
    }

      return v;
}

void hiss_val_print(hiss_val* val);

static void hiss_val_expr_print(hiss_val* v, char open, char close){
    unsigned int i;
    putchar(open);

    for(i = 0; i < v->count; i++){
        hiss_val_print(v->cells[i]);

        if(i != (v->count-1))
            putchar(' ');
    }
    putchar(close);
}

void hiss_val_print(hiss_val* val){
    switch(val->type){
        case HISS_NUM: printf("%li", val->num); break;
        case HISS_ERR: printf("%s Error: %s", HISS_ERR_TOKEN, val->err); break;
        case HISS_SYM: printf("%s", val->sym); break;
        case HISS_SEXPR: hiss_val_expr_print(val, '(', ')'); break;
    }
}

void hiss_val_num_println(hiss_val* val){ 
    hiss_val_print(val);
    putchar('\n');
}

void hiss_del(hiss_val* val){
    unsigned int i;
    switch(val->type){
        case HISS_NUM: break;
        case HISS_ERR: free(val->err); break;
        case HISS_SYM: free(val->sym); break;
        case HISS_SEXPR:
            for(i = 0; i < val->count; i++){
                hiss_del(val->cells[i]);
            }
            free(val->cells);
            break;
    }
    free(val);
}

#endif
