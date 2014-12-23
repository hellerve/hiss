#ifndef TYPES_H
#define TYPES_H

struct hiss_val;
struct hiss_env;
typedef struct hiss_val hiss_val;
typedef struct hiss_env hiss_env;
typedef hiss_val*(*hiss_builtin)(hiss_env*, hiss_val*);

struct hiss_val {
    unsigned short type;
    unsigned short marked;
    long num;
    unsigned short boolean;
    char* err;
    char* sym;
    char* str;
    hiss_builtin fun;
    hiss_env* env;
    hiss_val* formals;
    hiss_val* body;
    unsigned int count;
    struct hiss_val** cells;
};

struct hiss_env {
  hiss_env* par;
  unsigned int count;
  unsigned int max;
  char** syms;
  hiss_val** vals;
};

#endif
