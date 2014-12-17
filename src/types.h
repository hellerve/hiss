#ifndef TYPES_H
#define TYPES_H

#include "util.h"

enum {HISS_NUM, HISS_ERR};

enum {HISS_ZERO_DIV, HISS_BAD_OP, HISS_BAD_NUM};

typedef struct {
    unsigned short type;
    long num;
    int err;
} hiss_num;

hiss_num hiss_number(long n){
    hiss_num h;
    h.type = HISS_NUM;
    h.num = n;
    return h;
}

hiss_num hiss_err(int code){
    hiss_num h;
    h.type = HISS_ERR;
    h.err = code;
    return h;
}

void hiss_print(hiss_num num){
    switch(num.type){
        case HISS_NUM: 
            printf("%li", num.num);
            break;
        case HISS_ERR:
            if(num.err == HISS_ZERO_DIV)
                printf("%s Division By Zero.", HISS_ERR_TOKEN);
            else if(num.err == HISS_BAD_OP)
                printf("%s Invalid Operator.", HISS_ERR_TOKEN);
            else if(num.err == HISS_BAD_OP)
                printf("%s Invalid Number.", HISS_ERR_TOKEN);
            break;
    }
}


void hiss_println(hiss_num num){ 
    hiss_print(num);
    putchar('\n');
}

#endif
