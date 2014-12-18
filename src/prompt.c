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

char* eval(vpc_ast* t){
    hiss_val x = hiss_val_read(t);
    hiss_val_println(x);
    hiss_val_del(x);
}

int repl(){
    vpc_parser* number = vpc_new("number");
    vpc_parser* symbol = vpc_new("symbol");
    vpc_parser* s_expression  = vpc_new("s_expression");
    vpc_parser* q_expression  = vpc_new("q_expression");
    vpc_parser* expression   = vpc_new("expression");
    vpc_parser* hiss  = vpc_new("hiss");

    vpca_lang(VPCA_LANG_DEFAULT,
        "                                                             \
            number        : /-?[0-9]+/ ;                              \
            symbol        : \"list\" | \"head\" | \"tail\" | \"join\" \
                            | \"eval\" |'+' | '-' | '*' | '/' ;       \
            s_expression  : '(' <expr>* ')' ;                         \
            q_expression  : '{' <expr>* '}' ;                         \
            expression    : <number> | <symbol> | <sexpr> | <qexpr> ; \
        hiss          : /^/ <expr>* /$/ ;                             \
        ",
    number, symbol, s_expression, q_expression, expression, hiss);
    while(1){
        char* input = readline(PROMPT);
        
        add_history(input);

        if(strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0){
            free(input);
            break;
        }

        vpc_result r;

        if(vpc_parse("stdin", input, number, &r)){
            char* result = eval(r.output);
            printf("%s\n", result);
            vpc_ast_delete(r.output);
        } else {
            vpc_err_print(r.error);
            vpc_err_delete(r.error);
        }

        free(input);
    }

    vpc_cleanup(6, number, symbol, s_expression, q_expression, expression, hiss);

    return 0;
}

int main(int argc, char**argv){
    if(argc > 1)
        parse_arguments(argc, argv);

    print_header();
    
    return repl();
}
