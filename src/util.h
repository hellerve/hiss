#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdio.h>

#define HISS_ERR_TOKEN "[x]"
#define HISS_WARN_TOKEN "[-]"
#define GC_TRESHOLD 500

enum {HISS_ERR, HISS_NUM, HISS_BOOL, HISS_SYM, HISS_FUN, 
      HISS_SEXPR, HISS_QEXPR, HISS_STR};

enum {HISS_ZERO_DIV, HISS_BAD_OP, HISS_BAD_NUM};

enum {HISS_FALSE, HISS_TRUE};

static void inline die(char* message){
    fprintf(stderr, "%s\n", message);
    exit(0);
}
#endif
