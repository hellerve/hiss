#ifndef TYPES_H
#define TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

struct hiss_val;
struct hiss_env;
typedef struct hiss_val hiss_val;
typedef hiss_val*(*hiss_builtin)(struct hiss_env*, hiss_val*);

struct hiss_val {
    unsigned short type;
    long num;
    unsigned short boolean;
    char* err;
    char* sym;
    char* str;
    char* type_name;
    hiss_builtin fun;
    struct hiss_env* env;
    hiss_val* formals;
    hiss_val* body;
    unsigned int count;
    struct hiss_val** cells;
};

#endif
