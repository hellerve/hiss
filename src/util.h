#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdio.h>

static void inline die(char* message){
    fprintf(stderr, "%s\n", message);
    exit(0);
}
#endif
