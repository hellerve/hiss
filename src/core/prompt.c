#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "../utilities/type_utils.h"

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
#define USAGE "Usage: hiss [-hv]\n\tIf the program is called without arguments, "\
               "the REPL is started.\n\t-h triggers this help message.\n\t"\
               "-v triggers version information"

static __inline int ends_with(const char* str, const char* suffix){
    size_t lenstr = strlen(str);
    size_t lensfx = strlen(suffix);
    if(!str || !suffix)
        return HISS_FALSE;

    if(lensfx > lenstr)
        return HISS_FALSE;

    return strcmp(str + lenstr - lensfx, suffix) == 0;
}

static __inline char* parse_arguments(int argc, char** argv){
    int i;

    for(i = 1; i < argc; ){
        if(strcmp(argv[i], "-v") == 0){
            printf("%s\n", VERSION);
            exit(0);
        } if(ends_with(argv[i], ".his")){
            return argv[i];
        } else {
            puts(USAGE);
            exit(127);
        }
    }
    return NULL;
}

static __inline void print_header(){
    printf(VERSION);
#if defined(__GNUC__) || defined(__GNUG__)
    printf(", compiled with gcc %s\n", __VERSION__);
#elif defined(__clang__)
    printf(", compiled with clang %s\n", __clang_version__);
#elif defined(__ICC)
    printf(", compiled with icc %s\n", __ICC);
#elif defined(_MSC_VER)
    printf(", compiled with icc %s\n", _MSC_VER);
#endif
    printf("For exiting, press Ctrl-C or type exit/quit\n\n");
}

int repl(const char* f){
    vpc_result r;
    hiss_val* x = NULL;
    hiss_val* args = NULL;
    hiss_env* e = NULL;
    number = vpc_new("number");
    symbol = vpc_new("symbol");
    type = vpc_new("type");
    string = vpc_new("string");
    comment = vpc_new("comment");
    s_expression  = vpc_new("sexpr");
    q_expression  = vpc_new("qexpr");
    expression   = vpc_new("expr");
    hiss  = vpc_new("hiss");

    printf("Here!");

    vpca_lang(VPCA_LANG_DEFAULT,
        "number        : /-?[0-9]+/;                              \
         symbol        : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/;        \
         type          : /type:<symbol>/;                         \
         string        : /\"(\\\\.|[^\"])*\"/;                    \
         comment       : /#[^\\r\\n]*/;                           \
         sexpr         : '('<expr>*')';                           \
         qexpr         : '{'<expr>*'}';                           \
         expr          : <number> | <string> | <symbol> | <sexpr> | <qexpr> | <type> | <comment>;\
         hiss          : /^/<expr>*/$/;                           \
        ",
    number, symbol, type, string, comment, s_expression, q_expression, 
    expression, hiss);
    
    e = hiss_env_new();
    hiss_env_add_builtins(e);

    if(f){
        args = hiss_val_add(hiss_val_sexpr(), hiss_val_str(f));
        x = builtin_load(e, args);

        if(x->type == HISS_ERR) hiss_val_println(x);
        hiss_val_del(x);
    }else{
        while(1){
            char* input = readline(PROMPT);
        
            add_history(input);

            if(strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0){
                free(input);
                break;
            }

            if(vpc_parse("stdin", input, hiss, &r)){
                x = hiss_val_eval(e, hiss_val_read((vpc_ast*)r.output));
                hiss_env_add_type(e, x);
                hiss_val_println(x);
                hiss_val_del(x);
            
                vpc_ast_delete((vpc_ast*)r.output);
            } else {
                vpc_err_print(r.error);
                vpc_err_delete(r.error);
            }

            free(input);
        }
    }

    hiss_env_del(e);
    
    vpc_cleanup(6, number, symbol, string, comment, s_expression, 
                q_expression, expression, hiss);

    return 0;
}

int main(int argc, char**argv){
    char* f = NULL;
    if(argc > 1)
        f = parse_arguments(argc, argv);

    print_header();
    
    return repl(f);
}
