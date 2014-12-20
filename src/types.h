#ifndef TYPES_H
#define TYPES_H

#include <errno.h>
#include <string.h>

#include "parser.h"
#include "util.h"


#define HISS_ASSERT(args, cond, err) \
  if (!(cond)) { hiss_val_del(args); return hiss_err(err); }

enum {HISS_ERR, HISS_NUM, HISS_SYM, HISS_FUN, HISS_SEXPR, HISS_QEXPR};

enum {HISS_ZERO_DIV, HISS_BAD_OP, HISS_BAD_NUM};

struct hiss_val;
struct hiss_env;
typedef struct hiss_val hiss_val;
typedef struct hiss_env hiss_env;
typedef hiss_val*(*hiss_builtin)(hiss_env*, hiss_val*);

struct hiss_val {
    unsigned short type;
    long num;
    char* err;
    char* sym;
    hiss_builtin fun;
    unsigned int count;
    struct hiss_val** cells;
};

struct hiss_env {
  unsigned int count;
  char** syms;
  hiss_val** vals;
};


/*
 * Constructor functions
 */

hiss_env* hiss_env_new();
hiss_val* hiss_val_num(long n);
hiss_val* hiss_val_sym(const char* s);
hiss_val* hiss_val_fun(hiss_builtin fun);
hiss_val* hiss_val_sexpr();
hiss_val* hiss_val_qexpr();
hiss_val* hiss_err(const char* m);

/*
 * Helper functions
 */

hiss_val* hiss_val_add(hiss_val* v, hiss_val* a);
hiss_val* hiss_val_read_num(vpc_ast* t);
hiss_val* hiss_val_read(vpc_ast* t);
void hiss_val_print(hiss_val* val);
void hiss_val_println(hiss_val* val);
hiss_val* hiss_val_pop(hiss_val* v, unsigned int i);
hiss_val* hiss_val_take(hiss_val* v, unsigned int i);
hiss_val* hiss_val_copy(hiss_val* val);
hiss_val* hiss_env_get(hiss_env* e, hiss_val* k);
void hiss_env_put(hiss_env* e, hiss_val* k, hiss_val* v);


/*
 * Environment/extension functions
 */

void hiss_env_add_builtin(hiss_env* e, const char* name, hiss_builtin fun);
void hiss_env_add_builtins(hiss_env* e);

/*
 * Destructor functions
 */

void hiss_env_del(hiss_env* e);
void hiss_val_del(hiss_val* val);

/*
 * Evaluation functions
 */
hiss_val* hiss_val_eval_sexpr(hiss_env* e, hiss_val* v);
hiss_val* hiss_val_eval(hiss_env* e, hiss_val* v);

#endif