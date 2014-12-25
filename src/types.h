#ifndef TYPES_H
#define TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

struct hiss_val;
struct hiss_env;
typedef struct hiss_val hiss_val;
typedef struct hiss_env hiss_env;
typedef hiss_val*(*hiss_builtin)(hiss_env*, hiss_val*);

typedef struct hiss_entry{
    unsigned short marked;
    const hiss_val* key;
    const hiss_val* value;
    struct hiss_entry* next;
}hiss_entry;

typedef struct{
    unsigned int size;
    unsigned int n;
    hiss_entry** table;
}hiss_hashtable;

typedef struct hiss_type_entry{
    unsigned short marked;
    const char* key;
    const hiss_val* value;
    struct hiss_type_entry* next;
}hiss_type_entry;

typedef struct{
    unsigned int size;
    unsigned int n;
    hiss_type_entry** table;
}hiss_type_table;

struct hiss_val {
    unsigned short type;
    long num;
    unsigned short boolean;
    char* err;
    char* sym;
    char* str;
    char* type_name;
    hiss_builtin fun;
    hiss_env* env;
    hiss_val* formals;
    hiss_val* body;
    unsigned int count;
    struct hiss_val** cells;
};

struct hiss_env {
  hiss_env* par;
  hiss_type_table* types;
  hiss_hashtable* vals;
};

#endif
