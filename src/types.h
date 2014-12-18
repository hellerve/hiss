#ifndef TYPES_H
#define TYPES_H

#include <errno.h>
#include <string.h>

#include "parser.h"
#include "util.h"


#define HISS_ASSERT(args, cond, err) \
  if (!(cond)) { hiss_val_del(args); return hiss_err(err); }

enum {HISS_ERR, HISS_NUM, HISS_SYM, HISS_SEXPR, HISS_QEXPR};

enum {HISS_ZERO_DIV, HISS_BAD_OP, HISS_BAD_NUM};

typedef struct hiss_val {
    unsigned short type;
    long num;
    char* err;
    char* sym;
    unsigned int count;
    struct hiss_val** cells;
} hiss_val;

/*
 * Constructor functions
 */

hiss_val* hiss_val_num(long n);
hiss_val* hiss_val_sym(const char* s);
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
static void hiss_val_expr_print(hiss_val* v, const char open, const char close);
void hiss_val_println(hiss_val* val);
hiss_val* hiss_val_pop(hiss_val* v, unsigned int i);
hiss_val* hiss_val_take(hiss_val* v, unsigned int i);

/*
 * Destructor function
 */

void hiss_val_del(hiss_val* val);

/*
 * Evaluation functions
 */
hiss_val* hiss_val_eval_sexpr(hiss_val* val);
hiss_val* hiss_val_eval(hiss_val* val);
hiss_val* builtin_op(hiss_val* val, const char* op);

#endif
