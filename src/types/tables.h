#ifndef TABLES_H
#define TABLES_H

#ifdef __cplusplus
extern "C" {
#endif

struct hiss_val;

typedef struct hiss_type_entry{
    unsigned short marked;
    const char* key;
    const struct hiss_val* value;
    struct hiss_type_entry* next;
}hiss_type_entry;

typedef struct{
    unsigned int size;
    unsigned int n;
    hiss_type_entry** table;
}hiss_type_table;

typedef struct hiss_entry{
    unsigned short marked;
    const struct hiss_val* key;
    const struct hiss_val* value;
    struct hiss_entry* next;
}hiss_entry;

typedef struct{
    unsigned int size;
    unsigned int n;
    hiss_entry** table;
}hiss_hashtable;

#ifdef __cplusplus
}
#endif

#endif
