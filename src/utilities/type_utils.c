#include "type_utils.h"

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
static const char* hiss_type_name(int t);

hiss_val* hiss_val_add(hiss_val* v, hiss_val* a){
    v->count++;
    v->cells = (hiss_val**) realloc(v->cells, sizeof(hiss_val*) * v->count);
    v->cells[v->count-1] = a;
    return v;
}

hiss_env* hiss_env_copy(hiss_env* e){
    hiss_env* n = (hiss_env*) malloc(sizeof(hiss_env));
    n->par = e->par;
    n->vals = hiss_table_copy(e->vals);
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

static hiss_val* hiss_val_read_expr(vpc_ast* t){
    unsigned int i;
    hiss_val* v = NULL;
    if(strstr(t->tag, "qexpr")) v = hiss_val_qexpr();
    if(strcmp(t->tag, ">") == 0 || strstr(t->tag, "sexpr")) v = hiss_val_sexpr();

    for(i = 0; i < t->children_num; i++){
        if(strcmp(t->children[i]->contents, "(") == 0) continue;
        else if(strcmp(t->children[i]->contents, ")") == 0) continue;
        else if(strcmp(t->children[i]->contents, "}") == 0) continue;
        else if(strcmp(t->children[i]->contents, "{") == 0) continue; 
        else if(strcmp(t->children[i]->tag,  "regex") == 0) continue;
        else if(strstr(t->children[i]->tag, "comment")) continue;
        v = hiss_val_add(v, hiss_val_read(t->children[i]));
    }

    return v;
}

static hiss_val* hiss_val_read_type(vpc_ast* t){
    char* unescaped = NULL;
    hiss_val* type = NULL;
    
    t->contents[strlen(t->contents)-1] = '\0';
    
    unescaped = (char*) malloc(strlen(t->contents+1)+1);
    strcpy(unescaped, t->contents+1);
    unescaped = (char*) vpcf_unescape((vpc_val*)unescaped);
    type = hiss_val_type(unescaped, hiss_val_read_expr(t));

    free(unescaped);
    return type;
}

static hiss_val* hiss_val_read_str(vpc_ast* t){
    char* unescaped = NULL;
    hiss_val* str = NULL;
    
    t->contents[strlen(t->contents)-1] = '\0';
    
    unescaped = (char*) malloc(strlen(t->contents+1)+1);
    strcpy(unescaped, t->contents+1);
    unescaped = (char*) vpcf_unescape((vpc_val*)unescaped);
    str = hiss_val_str(unescaped);
    
    free(unescaped);
    return str;
}

hiss_val* hiss_val_read(vpc_ast* t){
    if(strstr(t->tag, "number")) return hiss_val_read_num(t);
    if(strstr(t->tag, "string")) return hiss_val_read_str(t);
    if(strstr(t->tag, "type")) return hiss_val_read_type(t);
    if(strstr(t->tag, "symbol")) return hiss_val_sym(t->contents);
    return hiss_val_read_expr(t);
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

static void hiss_val_print_str(hiss_val* v){
    char* escaped = (char*) malloc(strlen(v->str)+1);
    strcpy(escaped, v->str);

    escaped = (char*) vpcf_escape((vpc_val*)escaped);
    printf("\"%s\"", escaped);

    free(escaped);
}

void hiss_val_print(hiss_val* val){
    switch(val->type){
        case HISS_NUM: printf("%li", val->num); break;
        case HISS_STR: hiss_val_print_str(val); break;
        case HISS_BOOL: val->num == HISS_TRUE ? printf("true") : printf("false"); break;
        case HISS_ERR: printf("%s Error: %s", HISS_ERR_TOKEN, val->err); break;
        case HISS_SYM: printf("%s", val->sym); break;
        case HISS_SEXPR: hiss_val_expr_print(val, '(', ')'); break;
        case HISS_QEXPR: hiss_val_expr_print(val, '{', '}'); break;
        case HISS_USR: 
            printf("<type %s: ", val->type_name);
            hiss_val_print(val->formals);
            putchar('>');
            break;
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

hiss_val* hiss_val_pop(hiss_val* val, unsigned int i){
  hiss_val* x = val->cells[i];

  memmove(&val->cells[i], &val->cells[i+1],
          sizeof(hiss_val*) * (val->count-i-1));

  val->count--;

  val->cells = (hiss_val**) realloc(val->cells, sizeof(hiss_val*) * val->count);
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

hiss_val* builtin_ord(hiss_env* e, hiss_val* a, const char* op){
  unsigned short r;
  HISS_ASSERT_NUM(op, a, 2);
  HISS_ASSERT_TYPE(op, a, 0, HISS_NUM);
  HISS_ASSERT_TYPE(op, a, 1, HISS_NUM);

  if(strcmp(op, ">")  == 0)
    r = (a->cells[0]->num >  a->cells[1]->num);
  else if(strcmp(op, "<")  == 0)
    r = (a->cells[0]->num <  a->cells[1]->num);
  else if(strcmp(op, ">=") == 0)
    r = (a->cells[0]->num >= a->cells[1]->num);
  else if(strcmp(op, "<=") == 0)
    r = (a->cells[0]->num <= a->cells[1]->num);
  else if(strcmp(op, "||") == 0)
    r = (a->cells[0]->num || a->cells[1]->num);
  else if(strcmp(op, "&&") == 0)
    r = (a->cells[0]->num && a->cells[1]->num);
  else
    r = 0;


  hiss_val_del(a);
  return r == 0 ? hiss_val_bool(HISS_FALSE) : hiss_val_bool(HISS_TRUE);
}

static hiss_val* hiss_val_eq(hiss_val* x, hiss_val* y){
  unsigned int i;
  if (x->type != y->type) return hiss_val_bool(HISS_FALSE);

  switch (x->type){
    case HISS_NUM: return hiss_val_bool(x->num == y->num);
    case HISS_USR: return hiss_val_bool(x->type_name == y->type_name);
    case HISS_STR: return hiss_val_bool(!(strcmp(x->str, y->str) == 0));
    case HISS_ERR: return hiss_val_bool(strcmp(x->err, y->err) == 0);
    case HISS_SYM: return hiss_val_bool(strcmp(x->sym, y->sym) == 0);
    case HISS_FUN:
      if (x->fun || y->fun)
        return hiss_val_bool(x->fun == y->fun);
      else
        return hiss_val_bool(hiss_val_eq(x->formals, y->formals) 
          && hiss_val_eq(x->body, y->body));
    case HISS_QEXPR:
    case HISS_SEXPR:
      if (x->count != y->count) return hiss_val_bool(HISS_FALSE);
      for (i = 0; i < x->count; i++)
        if (!hiss_val_eq(x->cells[i], y->cells[i])) return hiss_val_bool(HISS_FALSE); 
      return hiss_val_bool(HISS_TRUE);
      break;
    case HISS_BOOL:
      return hiss_val_bool(x->boolean == y->boolean);
  }
  return hiss_val_bool(HISS_FALSE);
}

hiss_val* builtin_cmp(hiss_env* e, hiss_val* a, const char* op){
    hiss_val* r;
    HISS_ASSERT_NUM(op, a, 2);
    
    if(strcmp(op, "==") == 0){
        r =  hiss_val_eq(a->cells[0], a->cells[1]);
    }else{
        r = hiss_val_eq(a->cells[0], a->cells[1]);
        r->boolean = !(r->boolean);
    }
    
    hiss_val_del(a);
    return r;
}

static hiss_val* builtin_gt(hiss_env* e, hiss_val* a){
    return builtin_ord(e, a, ">");
}

static hiss_val* builtin_lt(hiss_env* e, hiss_val* a){
    return builtin_ord(e, a, "<");
}

static hiss_val* builtin_ge(hiss_env* e, hiss_val* a){
      return builtin_ord(e, a, ">=");
}

static hiss_val* builtin_le(hiss_env* e, hiss_val* a){
      return builtin_ord(e, a, "<=");
}

static hiss_val* builtin_eq(hiss_env* e, hiss_val* a){
      return builtin_ord(e, a, "==");
}

static hiss_val* builtin_ne(hiss_env* e, hiss_val* a){
    return builtin_cmp(e, a, "!=");
}

static hiss_val* builtin_or(hiss_env* e, hiss_val* a){
      return builtin_ord(e, a, "||");
}

static hiss_val* builtin_and(hiss_env* e, hiss_val* a){
      return builtin_ord(e, a, "&&");
}

static hiss_val* builtin_not(hiss_env* e, hiss_val* a){
      if(a->type == HISS_BOOL)
          return a->boolean ? hiss_val_bool(HISS_TRUE) : hiss_val_bool(HISS_FALSE);
      if(a->type == HISS_NUM)
          return hiss_val_num(!a->num);
      return hiss_err("'not' can only be applied to booleans or numbers, but got an %s.", 
                      hiss_type_name(a->type));
}

static hiss_val* builtin_if(hiss_env* e, hiss_val* a){
    hiss_val* x;

    if(a->count == 3){
        HISS_ASSERT_TYPE("if", a, 0, HISS_NUM);
        HISS_ASSERT_TYPE("if", a, 1, HISS_QEXPR);
        HISS_ASSERT_TYPE("if", a, 2, HISS_QEXPR);
    } 
    else if(a->count == 1){
        HISS_ASSERT_TYPE("if", a, 0, HISS_BOOL);
    }
    else{
        HISS_ASSERT(a, HISS_FALSE, "'if' takes either one argument(a boolean) or three(a comparison), but got %i.", a->count);
    }
  
    a->cells[1]->type = HISS_SEXPR;
    a->cells[2]->type = HISS_SEXPR;
  
    if(a->cells[0]->count)
        x = hiss_val_eval(e, hiss_val_pop(a, 1));
    else
        x = hiss_val_eval(e, hiss_val_pop(a, 2));
  
    hiss_val_del(a);
    return x;
}

hiss_val* hiss_val_copy(const hiss_val* val){
  unsigned int i;
  hiss_val* c = (hiss_val*) malloc(sizeof(hiss_val));
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
    case HISS_USR: 
        c->type_name = val->type_name; 
        c->formals = hiss_val_copy(val->formals);
        break;
    case HISS_STR: 
        c->str = (char*) malloc(strlen(val->str + 1)); 
        strcpy(c->str, val->str);
        break;
    case HISS_BOOL: c->boolean = val->boolean; break;
    case HISS_ERR:
      c->err = (char*) malloc(strlen(val->err) + 1);
      strcpy(c->err, val->err); 
      break;
    case HISS_SYM:
      c->sym = (char*) malloc(strlen(val->sym) + 1);
      strcpy(c->sym, val->sym); 
      break;
    case HISS_SEXPR:
    case HISS_QEXPR:
      c->count = val->count;
      c->cells = (hiss_val**) malloc(sizeof(hiss_val*) * c->count);
      for(i = 0; i < c->count; i++)
        c->cells[i] = hiss_val_copy(val->cells[i]);
    break;
  }
  
  return c;
}

hiss_val* hiss_env_get(hiss_env* e, hiss_val* k){
  hiss_val* v = hiss_val_copy(hiss_table_get(e->vals, k));

  if(v) return v;

  if(e->par)
      return hiss_env_get(e->par, k);
  
  return hiss_err("unbound symbol: %s", k->sym);
}

hiss_val* hiss_env_type_get(hiss_env* e, const char* k){
  hiss_val* v = hiss_val_copy(hiss_type_get(e->types, k));

  if(v) return v;

  if(e->par)
      return hiss_env_type_get(e->par, k);
  
  return hiss_err("unbound symbol: %s", k);
}

void hiss_env_put(hiss_env* e, hiss_val* k, hiss_val* v){
  hiss_table_insert(e->vals, k, v);
}

void hiss_env_type_put(hiss_env* e, const char* k, hiss_val* v){
  hiss_type_insert(e->types, k, v);
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
  hiss_val* result = NULL;

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

  result = hiss_val_call(e, f, v);
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

static hiss_val* builtin_true(hiss_env* e, hiss_val* a){
  return hiss_val_bool(HISS_TRUE);
}

static hiss_val* builtin_false(hiss_env* e, hiss_val* a){
  return hiss_val_bool(HISS_FALSE);
}

static hiss_val* builtin_lambda(hiss_env* e, hiss_val* a){
  unsigned int i;
  hiss_val* formals = NULL;
  hiss_val* body = NULL;

  HISS_ASSERT_NUM("lambda", a, 2);
  HISS_ASSERT_TYPE("lambda", a, 0, HISS_QEXPR);
  HISS_ASSERT_TYPE("lambda", a, 1, HISS_QEXPR);
  
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
    unsigned int i;
    int def = strcmp(fun, "def");
    int equals = strcmp(fun, "=");
    hiss_val* syms = NULL;

    HISS_ASSERT_TYPE(fun, a, 0, HISS_QEXPR);

    syms = a->cells[0];

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

static hiss_val* builtin_type(hiss_env* e, hiss_val* a){
    hiss_val *v;

    HISS_ASSERT_NUM("type?", a, 1);

    if(a->type != HISS_USR) return hiss_val_str(hiss_type_name(a->type));

    v = hiss_env_type_get(e, a->type_name);
    if(v){
        return hiss_val_str(a->type_name);
    }
    
    return hiss_val_bool(HISS_FALSE);
}

static hiss_val* builtin_shell(hiss_env* e, hiss_val* a){
    unsigned int i;
    int status;

    for(i = 0; i < a->count; i++)
        HISS_ASSERT_TYPE("shell", a, i, HISS_STR);

    for(i = 0; i < a->count; i++){
        status = system(a->cells[i]->str);
        if(status == -1) return hiss_err("system call errored.");
    }

    return hiss_val_num(status);
}


void hiss_env_add_builtin(hiss_env* e, const char* name, hiss_builtin fun){
  hiss_val* k = hiss_val_sym(name);
  hiss_val* v = hiss_val_fun(fun);
  hiss_env_put(e, k, v);
  hiss_val_del(k); 
  hiss_val_del(v);
}

void hiss_env_add_type(hiss_env* e, hiss_val* type){
    if(type->type != HISS_USR) return;

    hiss_env_type_put(e, type->type_name, type);
}

static hiss_val* builtin_const(hiss_env* e, hiss_val* a){
  hiss_val* v = NULL;
  hiss_val* formals = NULL;
  unsigned int actual = a->count-1;
  unsigned int expected;
  HISS_ASSERT_TYPE("const", a, 0, HISS_SYM);

  v = hiss_env_type_get(e, a->cells[0]->sym);

  if(v){
      expected = v->formals->count;
      
      if(expected != actual) 
          return hiss_err("Type %s expects %i arguments, got %i", a->cells[0]->sym, expected, actual);

    formals = hiss_val_pop(v, 0);
    hiss_val_del(v);
  
     //And now?
  }
  return NULL;
}

static hiss_val* builtin_from(hiss_env* e, hiss_val* a){
  hiss_val* x = NULL;
  hiss_val* y = NULL;
  unsigned int i;
  unsigned short found = HISS_FALSE;
  if(a->cells[0]->type == HISS_SYM && a->cells[1]->type == HISS_SYM){
    x = hiss_env_get(e, a->cells[0]);
    
    if(x->type == HISS_ERR){
        x = hiss_err("Symbol %s could not be found in environment.", a->cells[0]->sym);
    } else {
        for(i = 0; i < x->formals->count; i++){
            y = hiss_val_pop(x->formals, 0);
            if(strcmp(y->sym, a->cells[1]->sym) == 0){
                x = y;
                found = HISS_TRUE;
                break;
            }
        }
        if(!found)
            x = hiss_err("Symbol %s could not be found in %s.", a->cells[0]->sym, x->sym);
    }

    hiss_val_del(a);
    hiss_val_del(y);
    return x;
  }
  return hiss_err("Both arguments to from must be symbols.");
}

void hiss_env_add_builtins(hiss_env* e){  
  hiss_env_add_builtin(e, "def", builtin_def);
  hiss_env_add_builtin(e, "=", builtin_put);
  hiss_env_add_builtin(e, "lambda", builtin_lambda);

  hiss_env_add_builtin(e, "if", builtin_if);
  hiss_env_add_builtin(e, "==", builtin_eq);
  hiss_env_add_builtin(e, "!=", builtin_ne);
  hiss_env_add_builtin(e, ">",  builtin_gt);
  hiss_env_add_builtin(e, "<",  builtin_lt);
  hiss_env_add_builtin(e, ">=", builtin_ge);
  hiss_env_add_builtin(e, "<=", builtin_le);

  hiss_env_add_builtin(e, "true", builtin_true);
  hiss_env_add_builtin(e, "false", builtin_false);

  hiss_env_add_builtin(e, "||", builtin_or);
  hiss_env_add_builtin(e, "&&", builtin_and);
  hiss_env_add_builtin(e, "!", builtin_not);

  hiss_env_add_builtin(e, "list", builtin_list);
  hiss_env_add_builtin(e, "head", builtin_head);
  hiss_env_add_builtin(e, "tail", builtin_tail);
  hiss_env_add_builtin(e, "eval", builtin_eval);
  hiss_env_add_builtin(e, "join", builtin_join);
  hiss_env_add_builtin(e, "type?", builtin_type);
  hiss_env_add_builtin(e, "const", builtin_const);
  hiss_env_add_builtin(e, "from", builtin_from);
  hiss_env_add_builtin(e, "shell", builtin_shell);

  hiss_env_add_builtin(e, "+", builtin_add);
  hiss_env_add_builtin(e, "-", builtin_sub);
  hiss_env_add_builtin(e, "*", builtin_mul);
  hiss_env_add_builtin(e, "/", builtin_div);
}

const char* hiss_type_name(int t){
    switch(t){
        case HISS_FUN: return "Function";
        case HISS_USR: return "User defined";
        case HISS_STR: return "String";
        case HISS_BOOL: return "Boolean";
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
    hiss_val* sym = NULL;
    hiss_val* nsym = NULL;
    hiss_val* val = NULL;

    if(f->fun) return f->fun(e, a);

    while(a->count){
        sym = hiss_val_pop(f->formals, 0);

        val = hiss_val_pop(a, 0);

        if (strcmp(sym->sym, "&") == 0) {
            if (f->formals->count != 1) {
                hiss_val_del(a);
                return hiss_err("Function format invalid. Symbol '&' not followed by single symbol.");
            }

            nsym = hiss_val_pop(f->formals, 0);
            hiss_env_put(f->env, nsym, builtin_list(e, a));
            hiss_val_del(sym); 
            hiss_val_del(nsym);
            break;
        }

        if(expected == 0){
            hiss_val_del(a);
            return hiss_err("Function passed too many arguments. Got %i, expected %i.",
                            actual, expected);
        }


        hiss_env_put(f->env, sym, val);

        hiss_val_del(sym);
        hiss_val_del(val);
    }

    hiss_val_del(a);

    if(f->formals->count > 0 && strcmp(f->formals->cells[0]->sym, "&") == 0){
        if(f->formals->count != 2)
            return hiss_err("Function format invalid. Symbol '&' not followed by single symbol.");

        hiss_val_del(hiss_val_pop(f->formals, 0));

        sym = hiss_val_pop(f->formals, 0);
        val = hiss_val_qexpr();

        hiss_env_put(f->env, sym, val);
        hiss_val_del(sym); 
        hiss_val_del(val);
    }

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
