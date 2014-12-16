#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdio.h>

#define HISS_ERR_TOKEN "[x]"
#define HISS_WARN_TOKEN "[-]"

static void inline die(char* message){
    fprintf(stderr, "%s\n", message);
    exit(0);
}
#endif
