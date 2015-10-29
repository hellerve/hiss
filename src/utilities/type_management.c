#include "type_management.h"

hiss_val* hiss_val_num(long n){
    hiss_val* val = (hiss_val*) malloc(sizeof(hiss_val));
    val->type = HISS_NUM;
    val->num = n;
    return val;
}

hiss_val* hiss_val_bool(unsigned short boolean){
    hiss_val* val = (hiss_val*) malloc(sizeof(hiss_val));
    val->type = HISS_BOOL;
    val->boolean = boolean;
    return val;
}

hiss_val* hiss_val_sym(const char* s){
    hiss_val* val = (hiss_val*) malloc(sizeof(hiss_val));
    val->type = HISS_SYM;
    val->sym = (char*) malloc(strlen(s) + 1);
    strcpy(val->sym, s);
    return val;
}

hiss_val* hiss_val_str(const char* s){
    hiss_val* val = (hiss_val*) malloc(sizeof(hiss_val));
    val->type = HISS_STR;
    val->str = (char*) malloc(strlen(s) + 1);
    strcpy(val->str, s);
    return val;
}

hiss_val* hiss_val_fun(hiss_builtin fun) {
  hiss_val* val = (hiss_val*) malloc(sizeof(hiss_val));
  val->type = HISS_FUN;
  val->fun = fun;
  return val;
}

hiss_val* hiss_val_sexpr(){
    hiss_val* val = (hiss_val*) malloc(sizeof(hiss_val));
    val->type = HISS_SEXPR;
    val->type = HISS_SEXPR;
    val->count = 0;
    val->cells = NULL;
    return val;
}

hiss_val* hiss_val_qexpr(){
  hiss_val* v = (hiss_val*) malloc(sizeof(hiss_val));
  v->type = HISS_QEXPR;
  v->count = 0;
  v->cells = NULL;
  return v;
}

hiss_val* hiss_val_type(char* type, hiss_val* formals){
  hiss_val* v = (hiss_val*) malloc(sizeof(hiss_val));
  v->type = HISS_USR;
  v->type_name = type;
  v->formals = formals;
  v->count = 0;
  return v;
}

hiss_val* hiss_val_lambda(hiss_val* formals, hiss_val* body){
  hiss_val* v = (hiss_val*) malloc(sizeof(hiss_val));
  v->type = HISS_FUN;
  v->fun = NULL;
  v->env = hiss_env_new();
  v->formals = formals;
  v->body = body;
  return v;  
}

hiss_val* hiss_err(const char* fmt, ...){
    hiss_val* val = (hiss_val*) malloc(sizeof(hiss_val));
    va_list va;
    va_start(va, fmt);
    val->type = HISS_ERR;
    val->err = (char*) malloc(512);

    vsnprintf(val->err, 511, fmt, va);

    val->err = (char*) realloc(val->err, strlen(val->err)+1);

    return val;
}

void hiss_val_del(hiss_val* val){
    unsigned int i;
    switch(val->type){
        case HISS_BOOL:
        case HISS_NUM: break;
        case HISS_STR: free(val->str); break;
        case HISS_USR: 
            free(val->type_name); 
            hiss_val_del(val->formals);
            break;
        case HISS_ERR: free(val->err); break;
        case HISS_SYM: free(val->sym); break;
        case HISS_QEXPR:
        case HISS_SEXPR:
            for(i = 0; i < val->count; i++)
                hiss_val_del(val->cells[i]);
            free(val->cells);
            break;
        case HISS_FUN:
            if(!val->fun){
                hiss_env_del(val->env);
                hiss_val_del(val->formals);
                hiss_val_del(val->body);
            }
            break;
        default: break;
    }
    free(val);
}

