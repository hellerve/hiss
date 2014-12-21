#include "types.h"

#define HISS_ASSERT(args, cond, fmt, ...) \
  if (!(cond)) { hiss_val* err = hiss_err(fmt, __VA_ARGS__); hiss_val_del(args); return err;}

#define HISS_ASSERT_TYPE(fun, args, index, expect) \
  HISS_ASSERT(args, args->cells[index]->type == expect, \
    "Function '%s' passed incorrect type for argument %i. " \
    "Got %s, expected %s.", \
    fun, index, hiss_type_name(args->cells[index]->type), hiss_type_name(expect))

#define HISS_ASSERT_NUM(fun, args, num) \
  HISS_ASSERT(args, args->count == num, \
    "Function '%s' passed incorrect number of arguments. " \
    "Got %i, expected %i.", \
    fun, args->count, num)

#define HISS_ASSERT_NOT_EMPTY(fun, args, index) \
  HISS_ASSERT(args, args->cells[index]->count != 0, \
    "Function '%s' passed {} for argument %i.", fun, index);

static hiss_val* hiss_val_call(hiss_env* e, hiss_val* f, hiss_val* a);
const char* hiss_type_name(int t);

hiss_env* hiss_env_new(){
  hiss_env* e = malloc(sizeof(hiss_env));
  e->par = NULL;
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

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

hiss_val* hiss_val_fun(hiss_builtin fun) {
  hiss_val* val = malloc(sizeof(hiss_val));
  val->type = HISS_FUN;
  val->fun = fun;
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

hiss_val* hiss_val_lambda(hiss_val* formals, hiss_val* body){
  hiss_val* v = malloc(sizeof(hiss_val));
  v->type = HISS_FUN;
  v->fun = NULL;
  v->env = hiss_env_new();
  v->formals = formals;
  v->body = body;
  return v;  
}

hiss_val* hiss_err(const char* fmt, ...){
    hiss_val* val = malloc(sizeof(hiss_val));
    va_list va;
    va_start(va, fmt);
    val->type = HISS_ERR;
    val->err = malloc(512);
    
    vsnprintf(val->err, 511, fmt, va);

    val->err = realloc(val->err, strlen(val->err)+1);

    return val;
}

hiss_val* hiss_val_add(hiss_val* v, hiss_val* a){
    v->count++;
    v->cells = realloc(v->cells, sizeof(hiss_val*) * v->count);
    v->cells[v->count-1] = a;
    return v;
}

hiss_env* hiss_env_copy(hiss_env* e){
    int i;
    hiss_env* n = malloc(sizeof(hiss_env));
    n->par = e->par;
    n->count = e->count;
    n->syms = malloc(sizeof(char*) * n->count);
    n->vals = malloc(sizeof(hiss_val*) * n->count);
    for(i = 0; i < e->count; i++){
        n->syms[i] = malloc(strlen(e->syms[i]) + 1);
        strcpy(n->syms[i], e->syms[i]);
        n->vals[i] = hiss_val_copy(e->vals[i]);
    }
    return n;
}

static void hiss_env_def(hiss_env* e, hiss_val* k, hiss_val* v){
    while(e->par) e = e->par;
    hiss_env_put(e, k, v);
}

hiss_val* hiss_val_read_num(vpc_ast* t){
    long n = strtol(t->contents, NULL, 10);
    errno = 0;
    return errno != ERANGE ? hiss_val_num(n) : hiss_err((char *)"Invalid number.");
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
        case HISS_FUN: 
            if(val->fun){
                printf("<function>"); 
            }else {
                printf("(\\ ");
                hiss_val_print(val->formals);
                putchar(' ');
                hiss_val_print(val->body);
                putchar(')');
            }
            break;
    }
}

void hiss_val_println(hiss_val* val){ 
    hiss_val_print(val);
    putchar('\n');
}

void hiss_env_del(hiss_env* e){
  unsigned int i;
  for(i = 0; i < e->count; i++){
    free(e->syms[i]);
    hiss_val_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

void hiss_val_del(hiss_val* val){
    unsigned int i;
    switch(val->type){
        case HISS_NUM: break;
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

    }
    free(val);
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

static hiss_val* builtin_op(hiss_env*e, hiss_val* a, const char* op){
  unsigned int i;
  hiss_val* x = NULL;
  hiss_val* y = NULL;
  for(i = 0; i < a->count; i++){
    if (a->cells[i]->type != HISS_NUM) {
      hiss_val_del(a);
      return hiss_err("Cannot operate on non-number!");
    }
  }
  
  x = hiss_val_pop(a, 0);
  
  if((strcmp(op, "-") == 0) && a->count == 0) x->num = -x->num;
  
  while (a->count > 0){
    y = hiss_val_pop(a, 0);
    
    if(strcmp(op, "+") == 0) x->num += y->num;
    else if(strcmp(op, "-") == 0) x->num -= y->num;
    else if (strcmp(op, "*") == 0) x->num *= y->num;
    else if (strcmp(op, "/") == 0){
      if (y->num == 0) {
        hiss_val_del(x); 
        hiss_val_del(y);
        x = hiss_err("Division By Zero.");
        break;
      }
      x->num /= y->num;
    }
    
    hiss_val_del(y);
  }
  
  hiss_val_del(a);
  return x;
}

static hiss_val* builtin_head(hiss_env* e, hiss_val* a){    
  hiss_val* val = NULL;
  HISS_ASSERT_NUM("head", a, 1);
  HISS_ASSERT_TYPE("head", a, 0, HISS_QEXPR);
  HISS_ASSERT_NOT_EMPTY("head", a, 0);

  val = hiss_val_take(a, 0);
  while(val->count > 1) hiss_val_del(hiss_val_pop(val, 1));
  return val;
}

static hiss_val* builtin_tail(hiss_env* e, hiss_val* a){
  hiss_val* val = NULL;
  HISS_ASSERT_NUM("tail", a, 1);
  HISS_ASSERT_TYPE("tail", a, 0, HISS_QEXPR);
  HISS_ASSERT_NOT_EMPTY("tail", a, 0);

  val = hiss_val_take(a, 0);  
  hiss_val_del(hiss_val_pop(val, 0));
  return val;
}

static hiss_val* builtin_list(hiss_env* e, hiss_val* a) {
  a->type = HISS_QEXPR;
  return a;
}

static hiss_val* builtin_eval(hiss_env* e, hiss_val* a){
  hiss_val* x = NULL;
  HISS_ASSERT_NUM("eval", a, 1);
  HISS_ASSERT_TYPE("eval", a, 0, HISS_QEXPR);

  x = hiss_val_take(a, 0);
  x->type = HISS_SEXPR;
  return hiss_val_eval(e, x);
}

static hiss_val* hiss_val_join(hiss_env* e, hiss_val* x, hiss_val* y) {
  while (y->count)
    x = hiss_val_add(x, hiss_val_pop(y, 0));

  hiss_val_del(y);  
  return x;
}

static hiss_val* builtin_join(hiss_env*e, hiss_val* a) {
  unsigned int i; 
  hiss_val* x;
    
  for (i = 0; i < a->count; i++)
    HISS_ASSERT_TYPE("join", a, 0, HISS_QEXPR);

  x = hiss_val_pop(a, 0);

  while (a->count)
    x = hiss_val_join(e, x, hiss_val_pop(a, 0));

  hiss_val_del(a);
  return x;
}

hiss_val* hiss_val_copy(hiss_val* val){
  int i;
  hiss_val* c = malloc(sizeof(hiss_val));
  c->type = val->type;
  
  switch (val->type) {
    case HISS_FUN:
        if(val->fun){
            c->fun = val->fun;
        }else{
            c->fun = NULL;
            c->env = hiss_env_copy(val->env);
            c->formals = hiss_val_copy(val->formals);
            c->body = hiss_val_copy(val->body);
        }
        break;
    case HISS_NUM: c->num = val->num; break;
    case HISS_ERR:
      c->err = malloc(strlen(val->err) + 1);
      strcpy(c->err, val->err); 
      break;
    case HISS_SYM:
      c->sym = malloc(strlen(val->sym) + 1);
      strcpy(c->sym, val->sym); 
      break;
    case HISS_SEXPR:
    case HISS_QEXPR:
      c->count = val->count;
      c->cells = malloc(sizeof(hiss_val*) * c->count);
      for(i = 0; i < c->count; i++)
        c->cells[i] = hiss_val_copy(val->cells[i]);
    break;
  }
  
  return c;
}

hiss_val* hiss_env_get(hiss_env* e, hiss_val* k){
  unsigned int i;
  for(i = 0; i < e->count; i++)
    if (strcmp(e->syms[i], k->sym) == 0)
      return hiss_val_copy(e->vals[i]);

  if(e->par)
      return hiss_env_get(e->par, k);
  
  return hiss_err("unbound symbol: %s", k->sym);
}

void hiss_env_put(hiss_env* e, hiss_val* k, hiss_val* v){
  for(int i = 0; i < e->count; i++){
      if(strcmp(e->syms[i], k->sym) == 0){
        hiss_val_del(e->vals[i]);
        e->vals[i] = hiss_val_copy(v);
        return;
    }
  }

  e->count++;
  e->vals = realloc(e->vals, sizeof(hiss_val*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  e->vals[e->count-1] = hiss_val_copy(v);
  e->syms[e->count-1] = malloc(strlen(k->sym)+1);
  strcpy(e->syms[e->count-1], k->sym);
}

hiss_val* hiss_val_eval(hiss_env* e, hiss_val* v){
  if (v->type == HISS_SYM) {
    hiss_val* x = hiss_env_get(e, v);
    hiss_val_del(v);
    return x;
  }
  
  if(v->type == HISS_SEXPR) return hiss_val_eval_sexpr(e, v);
  return v;
}

hiss_val* hiss_val_eval_sexpr(hiss_env* e, hiss_val* v){
  unsigned int i;
  hiss_val* f = NULL;
  hiss_val* err = NULL;

  for(i = 0; i < v->count; i++)
    v->cells[i] = hiss_val_eval(e, v->cells[i]);
  
  for(i = 0; i < v->count; i++)
    if(v->cells[i]->type == HISS_ERR) return hiss_val_take(v, i);

  if(v->count == 0) return v; 
  if(v->count == 1) return hiss_val_take(v, 0);

  f = hiss_val_pop(v, 0);
  if(f->type != HISS_FUN){
    err = hiss_err("S-Expression starts with incorrect type. Got %s, expected %s.",
                   hiss_type_name(f->type), hiss_type_name(HISS_FUN));
    hiss_val_del(v); 
    hiss_val_del(f);
    return err;
  }

  hiss_val* result = hiss_val_call(e, f, v);
  hiss_val_del(f);
  return result;
}

static hiss_val* builtin_add(hiss_env* e, hiss_val* a){
  return builtin_op(e, a, "+");
}

static hiss_val* builtin_sub(hiss_env* e, hiss_val* a){
  return builtin_op(e, a, "-");
}

static hiss_val* builtin_mul(hiss_env* e, hiss_val* a){
  return builtin_op(e, a, "*");
}

static hiss_val* builtin_div(hiss_env* e, hiss_val* a){
  return builtin_op(e, a, "/");
}

static hiss_val* builtin_lambda(hiss_env* e, hiss_val* a){
  int i;
  hiss_val* formals = NULL;
  hiss_val* body = NULL;

  HISS_ASSERT_NUM("defun", a, 2);
  HISS_ASSERT_TYPE("defun", a, 0, HISS_QEXPR);
  HISS_ASSERT_TYPE("defun", a, 1, HISS_QEXPR);
  
  for (i = 0; i < a->cells[0]->count; i++)
    HISS_ASSERT(a, (a->cells[0]->cells[i]->type == HISS_SYM),
      "Cannot define non-symbol. Got %s, Expected %s.",
      hiss_type_name(a->cells[0]->cells[i]->type), hiss_type_name(HISS_SYM));
  
  formals = hiss_val_pop(a, 0);
  body = hiss_val_pop(a, 0);
  hiss_val_del(a);
  
  return hiss_val_lambda(formals, body);
}

static hiss_val* builtin_var(hiss_env* e, hiss_val* a, const char* fun){
    int i;
    int def = strcmp(fun, "def");
    int equals = strcmp(fun, "=");

    HISS_ASSERT_TYPE(fun, a, 0, HISS_QEXPR);

    hiss_val* syms = a->cells[0];

    for(i = 0; i < syms->count; i++)
        HISS_ASSERT(a, (syms->cells[i]->type == HISS_SYM),
                    "Function %s cannot define non-symbol. Got %s, expected %s", fun,
                    hiss_type_name(syms->cells[i]->type),
                    hiss_type_name(HISS_SYM));

    HISS_ASSERT(a, (syms->count == a->count-1),
                "Function %s passed too many arguments for symbols. Got %i, expected %i.",
                fun, syms->count, a->count-1);

    for(i = 0; i < syms->count; i++){
        if(def) hiss_env_def(e, syms->cells[i], a->cells[i+1]);
        if(equals) hiss_env_put(e, syms->cells[i], a->cells[i+1]);
    }

    hiss_val_del(a);

    return hiss_val_sexpr();
}

static hiss_val* builtin_def(hiss_env* e, hiss_val* a){
    return builtin_var(e, a, "def");
}

static hiss_val* builtin_put(hiss_env* e, hiss_val* a){
    return builtin_var(e, a, "=");
}

void hiss_env_add_builtin(hiss_env* e, const char* name, hiss_builtin fun){
  hiss_val* k = hiss_val_sym(name);
  hiss_val* v = hiss_val_fun(fun);
  hiss_env_put(e, k, v);
  hiss_val_del(k); 
  hiss_val_del(v);
}

void hiss_env_add_builtins(hiss_env* e){  
  hiss_env_add_builtin(e, "def", builtin_def);
  hiss_env_add_builtin(e, "=", builtin_put);
  hiss_env_add_builtin(e, "defun", builtin_lambda);

  hiss_env_add_builtin(e, "list", builtin_list);
  hiss_env_add_builtin(e, "head", builtin_head);
  hiss_env_add_builtin(e, "tail", builtin_tail);
  hiss_env_add_builtin(e, "eval", builtin_eval);
  hiss_env_add_builtin(e, "join", builtin_join);

  hiss_env_add_builtin(e, "+", builtin_add);
  hiss_env_add_builtin(e, "-", builtin_sub);
  hiss_env_add_builtin(e, "*", builtin_mul);
  hiss_env_add_builtin(e, "/", builtin_div);
}

const char* hiss_type_name(int t){
    switch(t){
        case HISS_FUN: return "Function";
        case HISS_NUM: return "Number";
        case HISS_ERR: return "Error";
        case HISS_SYM: return "Symbol";
        case HISS_SEXPR: return "S-Expression";
        case HISS_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

static hiss_val* hiss_val_call(hiss_env* e, hiss_val* f, hiss_val* a){
    unsigned int actual = a->count;
    unsigned int expected = f->formals->count;

    if(f->fun) return f->fun(e, a);

    while(a->count){
        if(f->formals->count == 0){
            hiss_val_del(a);
            return hiss_err("Function passed too many arguments. Got %i, expected %i.",
                            actual, expected);
        }

        hiss_val* sym = hiss_val_pop(f->formals, 0);

        hiss_val* val = hiss_val_pop(a, 0);

        hiss_env_put(f->env, sym, val);

        hiss_val_del(sym);
        hiss_val_del(val);
    }

    hiss_val_del(a);

    if(f->formals->count == 0){
        f->env->par = e;
        return builtin_eval(f->env, hiss_val_add(hiss_val_sexpr(), hiss_val_copy(f->body)));
    }
    
    return hiss_val_copy(f);
}

#undef HISS_ASSERT
#undef HISS_ASSERT_TYPE
#undef HISS_ASSERT_NUM
#undef HISS_ASSERT_NON_EMPTY
