#include "types.h"

hiss_val* hiss_val_num(long n){
    hiss_val* val = malloc(sizeof(hiss_val));
    val->type = HISS_NUM;
    val->num = n;
    return val;
}

hiss_val* hiss_val_sym(const char* s){
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

hiss_val* hiss_val_qexpr(){
  hiss_val* v = malloc(sizeof(hiss_val));
  v->type = HISS_QEXPR;
  v->count = 0;
  v->cells = NULL;
  return v;
}

hiss_val* hiss_err(const char* m){
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
    if(strstr(t->tag, "qexpr")) v = hiss_val_qexpr();

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

static void hiss_val_expr_print(hiss_val* v, const char open, const char close){
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
        case HISS_QEXPR: hiss_val_expr_print(val, '{', '}'); break;
    }
}

void hiss_val_println(hiss_val* val){ 
    hiss_val_print(val);
    putchar('\n');
}

void hiss_val_del(hiss_val* val){
    unsigned int i;
    switch(val->type){
        case HISS_NUM: break;
        case HISS_ERR: free(val->err); break;
        case HISS_SYM: free(val->sym); break;
        case HISS_QEXPR:
        case HISS_SEXPR:
            for(i = 0; i < val->count; i++){
                hiss_val_del(val->cells[i]);
            }
            free(val->cells);
            break;
    }
    free(val);
}

hiss_val* hiss_val_eval_sexpr(hiss_val* val){
    unsigned int i;
    hiss_val* result = NULL;
    hiss_val* f = NULL;

    for(i = 0; i < val->count; i++)
        val->cells[i] = hiss_val_eval(val->cells[i]);

    for(i = 0; i < val->count; i++)
        if(val->cells[i]->type == HISS_ERR) return hiss_val_take(val, i);

    if(val->count == 0) return val;
    else if(val->count == 1) return hiss_val_take(val, 0);

    f = hiss_val_pop(val, 0);
    if(f->type != HISS_SYM){
        hiss_val_del(f);
        hiss_val_del(val);
        return hiss_err("S-expession does not start with symbol!");
    }

    result = builtin_op(val, f->sym);
    hiss_val_del(f);
    return result;
}

hiss_val* hiss_val_eval(hiss_val* val){
    if(val->type == HISS_SEXPR) return hiss_val_eval_sexpr(val);
    return val;
}

hiss_val* hiss_val_pop(hiss_val* val, unsigned int i){
  hiss_val* x = val->cells[i];

  memmove(&val->cells[i], &val->cells[i+1],
          sizeof(hiss_val*) * (val->count-i-1));

  val->count--;

  val->cells = realloc(val->cells, sizeof(hiss_val*) * val->count);
  return x;
}

hiss_val* hiss_val_take(hiss_val* val, unsigned int i){
  hiss_val* x = hiss_val_pop(val, i);
  hiss_val_del(val);
  return x;
}

hiss_val* builtin_op(hiss_val* val, const char* op){
  unsigned int i;
  hiss_val *x = NULL;
  for(i = 0; i < val->count; i++){
    if(val->cells[i]->type != HISS_NUM){
      hiss_val_del(val);
      return hiss_err("Cannot operate on non-number!");
    }
  }
  
  x = hiss_val_pop(val, 0);

  if((strcmp(op, "-") == 0) && val->count == 0)
    x->num = -x->num;

  while(val->count > 0){
    hiss_val* y = hiss_val_pop(val, 0);

    if(strcmp(op, "+") == 0) x->num += y->num;
    else if(strcmp(op, "-") == 0) x->num -= y->num;
    else if(strcmp(op, "*") == 0) x->num *= y->num;
    else if(strcmp(op, "/") == 0){
      if(y->num == 0){
        hiss_val_del(x); hiss_val_del(y);
        x = hiss_err("Division By Zero!"); 
        break;
      }
      x->num /= y->num;
    }

    hiss_val_del(y);
  }

  hiss_val_del(val); 
  return x;
}

hiss_val* builtin_head(hiss_val* a){    
  hiss_val* val = NULL;
  HISS_ASSERT(a, a->count == 1, "Function 'head' passed too many arguments!");
  HISS_ASSERT(a, a->cells[0]->type == HISS_QEXPR, "Function 'head' passed incorrect type!");
  HISS_ASSERT(a, a->cells[0]->count != 0, "Function 'head' passed {}!");

  val = hiss_val_take(a, 0);
  while(val->count > 1) hiss_val_del(hiss_val_pop(val, 1));
  return val;
}

hiss_val* builtin_tail(hiss_val* a) {
  hiss_val* val = NULL;
  HISS_ASSERT(a, a->count == 1, "Function 'tail' passed too many arguments!");
  HISS_ASSERT(a, a->cells[0]->type == HISS_QEXPR, "Function 'tail' passed incorrect type!");
  HISS_ASSERT(a, a->cells[0]->count != 0, "Function 'tail' passed {}!");

  val = hiss_val_take(a, 0);  
  hiss_val_del(hiss_val_pop(val, 0));
  return val;
}