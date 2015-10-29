#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define HISS_ERR_TOKEN "[x]"
#define HISS_WARN_TOKEN "[-]"
#define GC_TRESHOLD 500

enum {HISS_ERR, HISS_NUM, HISS_BOOL, HISS_SYM, HISS_FUN, 
      HISS_SEXPR, HISS_QEXPR, HISS_STR, HISS_USR};

enum {HISS_FALSE, HISS_TRUE};

static __inline void die(char* message){
    fprintf(stderr, "%s\n", message);
    exit(0);
}

static __inline char *str_replace(char *orig, const char* rep, const char* with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    unsigned long len_rep;  // length of rep
    unsigned long len_with; // length of with
    unsigned long len_front; // distance between rep and end of last rep
    unsigned int count;    // number of replacements

    if (!orig)
        return NULL;
    if (!rep)
        rep = "";
    len_rep = strlen(rep);
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = (unsigned) (ins - orig);
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

#endif
