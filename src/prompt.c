#include <stdio.h>
#include <string.h>

#ifdef _WIN32

static char buffer[2048];

char* readline(char* prompt){
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* copy = malloc(stlren(buffer)+1);
    strcp(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy;
}

void add_history(char* unused){}

#else

#include <editline/readline.h>

#ifndef __APPLE__
#include <editline/history.h>
#endif
#endif

#include "util.h"

#define VERSION "Hiss version 0.0.1\n"
#define PROMPT "hiss> "
#define USAGE "Usage: hiss [-hv]\n\tIf the program is called without arguments, \
               the REPL is started.\n\t-h triggers this help message.\n\t\
               -v triggers version information\n"

static inline void parse_arguments(int argc, char** argv){
    int i;
    for(i = 1; i < argc; i++){
        if(strcmp(argv[argc], "-v")){
            printf(VERSION);
            exit(0);
        } else {
            printf(USAGE);
            exit(127);
        }
    }
}

static inline void print_header(){
    printf(VERSION);
    printf("For exiting, press Ctrl-C or type exit/quit\n\n");
}

int repl(){
    while(1){
        char* input = readline(PROMPT);
        
        add_history(input);

        if(strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0){
            free(input);
            break;
        }

        printf("You entered %s\n", input);

        free(input);
    }

    return 0;
}

int main(int argc, char**argv){
    if(argc > 1)
        parse_arguments(argc, argv);

    print_header();
    
    return repl();
}
