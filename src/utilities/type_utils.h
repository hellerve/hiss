#ifndef TYPE_UTILS
#define TYPE_UTILS

#include <errno.h>
#include <string.h>

#include "hiss_hash.h"
#include "hiss_type_table.h"
#include "util.h"
#include "type_management.h"

#include "../core/parser.h"
#include "../types/types.h"

#ifdef __cplusplus
extern "C" {
#endif

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
hiss_val* hiss_val_copy(const hiss_val* val);
hiss_val* hiss_env_get(hiss_env* e, hiss_val* k);
void hiss_env_put(hiss_env* e, hiss_val* k, hiss_val* v);


/*
 * Environment/extension functions
 */

void hiss_env_add_builtin(hiss_env* e, const char* name, hiss_builtin fun);
void hiss_env_add_builtins(hiss_env* e);
void hiss_env_add_type(hiss_env* e, hiss_val* a);
hiss_val* builtin_load(hiss_env* e, hiss_val* a);

/*
 * Evaluation functions
 */
hiss_val* hiss_val_eval_sexpr(hiss_env* e, hiss_val* v);
hiss_val* hiss_val_eval(hiss_env* e, hiss_val* v);

#ifdef __cplusplus
}
#endif

#endif
