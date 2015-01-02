#ifndef TYPE_MANAGEMENT_H
#define TYPE_MANAGEMENT_H

#include <stdarg.h>

#include "../types/environment.h"
#include "../types/tables.h"
#include "../types/types.h"

#include "util.h"


/*
 * Constructor functions
 */

hiss_val* hiss_val_num(long n);
hiss_val* hiss_val_bool(unsigned short n);
hiss_val* hiss_val_sym(const char* s);
hiss_val* hiss_val_str(const char* s);
hiss_val* hiss_val_fun(hiss_builtin fun);
hiss_val* hiss_val_lambda(hiss_val* formals, hiss_val* body);
hiss_val* hiss_val_type(char* type, hiss_val* formals);
hiss_val* hiss_val_sexpr();
hiss_val* hiss_val_qexpr();
hiss_val* hiss_err(const char* fmt, ...);

/*
 * Destructor functions
 */

void hiss_val_del(hiss_val* val);

#endif
