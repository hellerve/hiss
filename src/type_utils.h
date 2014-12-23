#ifndef TYPE_UTILS
#define TYPE_UTILS

#include <errno.h>
#include <string.h>

#include "parser.h"
#include "gc.h"
#include "util.h"

/*
 * Constructor functions
 */

hiss_env* hiss_env_new();
hiss_val* hiss_val_num(long n);
hiss_val* hiss_val_bool(unsigned short n);
hiss_val* hiss_val_sym(const char* s);
hiss_val* hiss_val_str(const char* s);
hiss_val* hiss_val_fun(hiss_builtin fun);
hiss_val* hiss_val_lambda(hiss_val* formals, hiss_val* body);
hiss_val* hiss_val_sexpr();
hiss_val* hiss_val_qexpr();
hiss_val* hiss_err(const char* fmt, ...);

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
hiss_val* builtin_load(hiss_env* e, hiss_val* a);

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
