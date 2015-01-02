#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "tables.h"

#include "../utilities/hiss_hash.h"
#include "../utilities/hiss_type_table.h"

struct hiss_env{
  struct hiss_env* par;
  hiss_type_table* types;
  hiss_hashtable* vals;
};

typedef struct hiss_env hiss_env;

/*
 * Constructor functions
 */

hiss_env* hiss_env_new();

/*
 * Destructor functions
 */

void hiss_env_del(hiss_env* e);

#endif
