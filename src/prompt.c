#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "types.h"

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

#define VERSION "Hiss version 0.0.1"
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
#if defined(__GNUC__) || defined(__GNUG__)
    printf(", compiled with gcc %s\n", __VERSION__);
#elif defined(__clang__)
    printf(", compiled with clang %s\n", __clang_version__);
#elif defined(__ICC)
    printf(", compiled with icc %s\n", __ICC);
#endif
    printf("For exiting, press Ctrl-C or type exit/quit\n\n");
}

char* eval(vp_ast* t){
    if(strstr(t->tag, "number"))
        return t->contents;
    else
        return "no match";
}

int repl(){
    vp_parser* num = vp_new("number");

    vpa_lang(VPA_LANG_DEFAULT, "number: /-?[0-9]+/ ;", num);
    while(1){
        char* input = readline(PROMPT);
        
        add_history(input);

        if(strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0){
            free(input);
            break;
        }

        vp_result r;

        if(vp_parse("stdin", input, number, &r)){
            char* result = eval(r.output);
            printf("%s\n", result);
            vp_ast_delete(r.output);
        } else {
            vp_err_print(r.error);
            vp_err_delete(r.error);
        }

        free(input);
    }

    vp_cleanup(1, num);

    return 0;
}

int main(int argc, char**argv){
    if(argc > 1)
        parse_arguments(argc, argv);

    print_header();
    
    return repl();
}
