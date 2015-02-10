#include "environment.h"

hiss_env* hiss_env_new(){
  hiss_env* e = (hiss_env*) malloc(sizeof(hiss_env));
  e->par = NULL;
  e->types = hiss_type_new();
  e->vals = hiss_table_new();
  return e;
}

void hiss_env_del(hiss_env* e){
  if(!e) return;

  hiss_table_delete(e->vals);
  hiss_type_delete(e->types);

  if(e->vals) free(e->vals);
  if(e->types) free(e->types);
  free(e);
}

