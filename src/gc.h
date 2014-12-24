#ifndef GC
#define GC

#include "types.h"
#include "util.h"
#include "hiss_hash.h"

#ifdef __cplusplus
extern "C" {
#endif

void gc(hiss_hashtable* t);

#ifdef __cplusplus
}
#endif

#endif
