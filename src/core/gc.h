#ifndef GC
#define GC

#include "../utilities/util.h"
#include "../utilities/hiss_hash.h"
#include "../types/tables.h"

#ifdef __cplusplus
extern "C" {
#endif

void gc(hiss_hashtable* t);

#ifdef __cplusplus
}
#endif

#endif
