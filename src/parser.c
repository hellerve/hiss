#include "parser.h"

/*
 *  Types and Definitions
 */
enum {VPC_INPUT_STRING, VPC_INPUT_FILE, VPC_INPUT_PIPE };

typedef struct{
    int type;
    char* filename;
    vpc_cur_state state;

    char* string;
    char* buffer;
    FILE* file;

    int backtrack;
    int marks_count;
    vpc_cur_state* marks;
    char* lasts;

    char last;
} vpc_input;

/*
 *  Static functions
 */

/*
 *  State functions
 */

static vpc_cur_state vpc_state_invalid(){
    vpc_cur_state s;
    s.pos = -1;
    s.row = -1;
    s.col = -1;
    return s;
}

static vpc_cur_state vpc_state_new(){
    vpc_cur_state s;
    s.pos = 0;
    s.row = 0;
    s.col = 0;
    return s;
}

static vpc_cur_state* vpc_state_copy(vpc_cur_state s){
    vpc_cur_state* r = malloc(sizeof(vpc_cur_state));
    memcpy(r, &s, sizeof(vpc_cur_state));
    return r;
}

/*
 *  Error functions
 */
static vpc_err* vpc_err_new(const char* filename, vpc_cur_state s, const char* expected, char recieved){
    vpc_err* err = malloc(sizeof(vpc_err));
    err->filename = malloc(strlen(filename) + 1);
    strcpy(err->filename, filename);
    err->state = s,
    err->expected_count = 1;
    err->expected = malloc(sizeof(char*));
    err->expected[0] = malloc(strlen(expected) + 1);
    strcpy(err->expected[0], expected);
    err->failure = NULL;
    err->recieved = recieved;
    return err;
}

static vpc_err* vpc_err_fail(const char* filename, vpc_cur_state s, const char* failure){
    vpc_err* err = malloc(sizeof(vpc_err));
    err->filename = malloc(strlen(filename) + 1);
    strcpy(err->filename, filename);
    err->state = s;
    err->expected_count = 0;
    err->expected = NULL;
    err->failure = malloc(strlen(failure) + 1);
    strcpy(err->failure, failure);
    err->recieved = ' ';
    return err;
}

static int vpc_err_contains_expected(vpc_err* container, char* expected){
    unsigned int i;
    for(i = 0; i < container->expected_count; i++)
        if(strcmp(container->expected[i], expected) == 0) return VPC_TRUE;

    return VPC_FALSE;
}

static void vpc_err_add_expected(vpc_err* add_to, char* expected){
    add_to->expected_count++;
    add_to->expected = realloc(add_to->expected, sizeof(char*) * (long unsigned int) add_to->expected_count);
    add_to->expected[add_to->expected_count-1] = malloc(strlen(expected) + 1);
    strcpy(add_to->expected[add_to->expected_count-1], expected);
}

static void vpc_err_clear_expected(vpc_err* clear_from, char* expected){
    unsigned int i;
    for(i = 0; i < clear_from->expected_count; i++)
        free(clear_from->expected[i]);
    
    clear_from->expected_count = 1;
    clear_from->expected = realloc(clear_from->expected, sizeof(char*) * (long unsigned int) clear_from->expected_count);
    clear_from->expected[0] = malloc(strlen(expected) + 1);
    strcpy(clear_from->expected[0], expected);
}

static void vpc_err_string_cat(char* buffer, int* pos, int* max, char const* fmt, ...){
    int left = ((*max) - (*pos));
    va_list va;
    va_start(va, fmt);
    if(left < 0) left = 0;
    *pos += vsprintf(buffer + (*pos), fmt, va);
    va_end(va);
}

static char char_unescape_buffer[3];
static const char *vpc_err_char_unescape(char c) {
    char_unescape_buffer[0] = '\'';
    char_unescape_buffer[1] = ' ';
    char_unescape_buffer[2] = '\'';

    switch (c) {
        case '\a': return "bell";
        case '\b': return "backspace";
        case '\f': return "formfeed";
        case '\r': return "carriage return";
        case '\v': return "vertical tab";
        case '\0': return "end of input";
        case '\n': return "newline";
        case '\t': return "tab";
        case ' ' : return "space";
        default:
            char_unescape_buffer[1] = c;
            return char_unescape_buffer;
    }
}

static vpc_err *vpc_err_or(vpc_err** es, int n){
    int i, j;
    vpc_err *e = malloc(sizeof(vpc_err));
    e->state = vpc_state_invalid();
    e->expected_count = 0;
    e->expected = NULL;
    e->failure = NULL;
    e->filename = malloc(strlen(es[0]->filename)+1);
    strcpy(e->filename, es[0]->filename);

    for(i = 0; i < n; i++)
        if(es[i]->state.pos > e->state.pos) e->state = es[i]->state;

    for( i = 0; i < n; i++){
        if(es[i]->state.pos < e->state.pos) continue;

        if(es[i]->failure){
            e->failure = malloc(strlen(es[i]->failure)+1);
            strcpy(e->failure, es[i]->failure);
            break;
        }

        e->recieved = es[i]->recieved;

        for(j = 0; j < es[i]->expected_count; j++)
            if(!vpc_err_contains_expected(e, es[i]->expected[j])) vpc_err_add_expected(e, es[i]->expected[j]);

    }

    for(i = 0; i < n; i++)
        vpc_err_delete(es[i]);

    return e;
}

static vpc_err *vpc_err_repeat(vpc_err* e, const char* prefix){
    unsigned int i;
    char *expect = malloc(strlen(prefix) + 1);
    strcpy(expect, prefix);

    if(e->expected_count == 1){
        expect = realloc(expect, strlen(expect) + strlen(e->expected[0]) + 1);
        strcat(expect, e->expected[0]);
    }

    if(e->expected_count > 1){
        for(i = 0; i < e->expected_count-2; i++){
            expect = realloc(expect, strlen(expect) + strlen(e->expected[i]) + strlen(", ") + 1);
            strcat(expect, e->expected[i]);
            strcat(expect, ", ");
        }

        expect = realloc(expect, strlen(expect) + strlen(e->expected[e->expected_count-2]) + strlen(" or ") + 1);
        strcat(expect, e->expected[e->expected_count-2]);
        strcat(expect, " or ");
        expect = realloc(expect, strlen(expect) + strlen(e->expected[e->expected_count-1]) + 1);
        strcat(expect, e->expected[e->expected_count-1]);
    }

    vpc_err_clear_expected(e, expect);
    free(expect);

    return e;
}

static vpc_err* vpc_err_many1(vpc_err* x){
    return vpc_err_repeat(x, "one or more of ");
}

static vpc_err *vpc_err_count(vpc_err* e, unsigned int n){
    vpc_err* c;
    unsigned int digits = n/10 + 1;
    char* prefix = malloc(digits + strlen(" of ") + 1);
    
    sprintf(prefix, "%i of ", n);
    c = vpc_err_repeat(e, prefix);
    
    free(prefix);
    
    return c;
}

/*
 *  Exported functions
 */

/*
 *  Error functions
 */

void vpc_err_delete(vpc_err *x) {
  unsigned int i;
  for (i = 0; i < x->expected_count; i++)
    free(x->expected[i]);
  
  free(x->expected);
  free(x->filename);
  free(x->failure);
  free(x);
}

void vpc_err_print_to(vpc_err* print, FILE* f){
    char* str = vpc_err_string(print);
    fprintf(f, "%s", str);
    free(str);
}

void vpc_err_print(vpc_err* print){
    vpc_err_print_to(print, stdout);
}

char *vpc_err_string(vpc_err *e){
    char* buffer = calloc(1, 1024);
    int max = 1023;
    int pos = 0;
    unsigned int i;

    if(e->failure){
        vpc_err_string_cat(buffer, &pos, &max, "%s: error: %s\n", e->filename, e->failure);
        return buffer;
    }

    vpc_err_string_cat(buffer, &pos, &max, "%s:%i:%i: error: expected ", e->filename, 
                       e->state.row+1, e->state.col+1);

    if(e->expected_count == 0) vpc_err_string_cat(buffer, &pos, &max, "ERROR: NOTHING EXPECTED");
    else if(e->expected_count == 1) vpc_err_string_cat(buffer, &pos, &max, "%s", e->expected[0]);
    else if(e->expected_count >= 2){ 
        for(i = 0; i < e->expected_count-2; i++)
            vpc_err_string_cat(buffer, &pos, &max, "%s, ", e->expected[i]);

        vpc_err_string_cat(buffer, &pos, &max, "%s or %s",
                e->expected[e->expected_count-2],
                e->expected[e->expected_count-1]);
    }

    vpc_err_string_cat(buffer, &pos, &max, " at ");
    vpc_err_string_cat(buffer, &pos, &max, vpc_err_char_unescape(e->recieved));
    vpc_err_string_cat(buffer, &pos, &max, "\n");

    return realloc(buffer, strlen(buffer)+1);

}

int vpc_parse(const char *filename, const char *string, vpc_parser *p, vpc_result *r);
int vpc_parse_file(const char *filename, FILE *file, vpc_parser *p, vpc_result *r);
int vpc_parse_pipe(const char *filename, FILE *pipe, vpc_parser *p, vpc_result *r);
int vpc_parse_contents(const char *filename, vpc_parser *p, vpc_result *r);

vpc_parser *vpc_new(const char *name);
vpc_parser *vpc_define(vpc_parser *p, vpc_parser *a);
vpc_parser *vpc_undefine(vpc_parser *p);

void vpc_delete(vpc_parser *p);
void vpc_cleanup(int n, ...);

vpc_parser *vpc_any(void);
vpc_parser *vpc_char(char c);
vpc_parser *vpc_range(char s, char e);
vpc_parser *vpc_oneof(const char *s);
vpc_parser *vpc_noneof(const char *s);
vpc_parser *vpc_satisfy(int(*f)(char));
vpc_parser *vpc_string(const char *s);

vpc_parser *vpc_pass(void);
vpc_parser *vpc_fail(const char *m);
vpc_parser *vpc_failf(const char *fmt, ...);
vpc_parser *vpc_lift(vpc_ctor f);
vpc_parser *vpc_lift_val(vpc_val *x);
vpc_parser *vpc_anchor(int(*f)(char,char));
vpc_parser *vpc_state(void);

vpc_parser *vpc_expect(vpc_parser *a, const char *e);
vpc_parser *vpc_expectf(vpc_parser *a, const char *fmt, ...);
vpc_parser *vpc_parse_apply(vpc_parser *a, vpc_apply f);
vpc_parser *vpc_parse_apply_to(vpc_parser *a, vpc_apply_to f, void *x);

vpc_parser *vpc_not(vpc_parser *a, vpc_dtor da);
vpc_parser *vpc_not_lift(vpc_parser *a, vpc_dtor da, vpc_ctor lf);
vpc_parser *vpc_maybe(vpc_parser *a);
vpc_parser *vpc_maybe_lift(vpc_parser *a, vpc_ctor lf);

vpc_parser *vpc_many(vpc_fold f, vpc_parser *a);
vpc_parser *vpc_many1(vpc_fold f, vpc_parser *a);
vpc_parser *vpc_count(int n, vpc_fold f, vpc_parser *a, vpc_dtor da);

vpc_parser *vpc_or(int n, ...);
vpc_parser *vpc_and(int n, vpc_fold f, ...);

vpc_parser *vpc_predictive(vpc_parser *a);

vpc_parser *vpc_eoi(void);
vpc_parser *vpc_soi(void);

vpc_parser *vpc_boundary(void);

vpc_parser *vpc_whitespace(void);
vpc_parser *vpc_whitespaces(void);
vpc_parser *vpc_blank(void);

vpc_parser *vpc_newline(void);
vpc_parser *vpcab(void);
vpc_parser *vpc_escape(void);

vpc_parser *vpc_digit(void);
vpc_parser *vpc_hexdigit(void);
vpc_parser *vpc_octdigit(void);
vpc_parser *vpc_digits(void);
vpc_parser *vpc_hexdigits(void);
vpc_parser *vpc_octdigits(void);

vpc_parser *vpc_lower(void);
vpc_parser *vpc_upper(void);
vpc_parser *vpc_alpha(void);
vpc_parser *vpc_underscore(void);
vpc_parser *vpc_alphanum(void);

vpc_parser *vpc_int(void);
vpc_parser *vpc_hex(void);
vpc_parser *vpc_oct(void);
vpc_parser *vpc_number(void);

vpc_parser *vpc_real(void);
vpc_parser *vpc_float(void);

vpc_parser *vpc_char_lit(void);
vpc_parser *vpc_string_lit(void);
vpc_parser *vpc_regex_lit(void);

vpc_parser *vpc_ident(void);

vpc_parser *vpc_startwith(vpc_parser *a);
vpc_parser *vpc_endwith(vpc_parser *a, vpc_dtor da);
vpc_parser *vpc_whole(vpc_parser *a, vpc_dtor da);

vpc_parser *vpc_stripl(vpc_parser *a);
vpc_parser *vpc_stripr(vpc_parser *a);
vpc_parser *vpc_strip(vpc_parser *a);
vpc_parser *vpcok(vpc_parser *a); 
vpc_parser *vpc_sym(const char *s);
vpc_parser *vpcotal(vpc_parser *a, vpc_dtor da);

vpc_parser *vpc_between(vpc_parser *a, vpc_dtor ad, const char *o, const char *c);
vpc_parser *vpc_parens(vpc_parser *a, vpc_dtor ad);
vpc_parser *vpc_braces(vpc_parser *a, vpc_dtor ad);
vpc_parser *vpc_brackets(vpc_parser *a, vpc_dtor ad);
vpc_parser *vpc_squares(vpc_parser *a, vpc_dtor ad);

vpc_parser *vpcok_between(vpc_parser *a, vpc_dtor ad, const char *o, const char *c);
vpc_parser *vpcok_parens(vpc_parser *a, vpc_dtor ad);
vpc_parser *vpcok_braces(vpc_parser *a, vpc_dtor ad);
vpc_parser *vpcok_brackets(vpc_parser *a, vpc_dtor ad);
vpc_parser *vpcok_squares(vpc_parser *a, vpc_dtor ad);

void vpcf_dtor_null(vpc_val *x);

vpc_val *vpcf_ctor_null(void);
vpc_val *vpcf_ctor_str(void);

vpc_val *vpcf_free(vpc_val *x);
vpc_val *vpcf_int(vpc_val *x);
vpc_val *vpcf_hex(vpc_val *x);
vpc_val *vpcf_oct(vpc_val *x);
vpc_val *vpcf_float(vpc_val *x);

vpc_val *vpcf_escape(vpc_val *x);
vpc_val *vpcf_escape_regex(vpc_val *x);
vpc_val *vpcf_escape_string_raw(vpc_val *x);
vpc_val *vpcf_escape_char_raw(vpc_val *x);

vpc_val *vpcf_unescape(vpc_val *x);
vpc_val *vpcf_unescape_regex(vpc_val *x);
vpc_val *vpcf_unescape_string_raw(vpc_val *x);
vpc_val *vpcf_unescape_char_raw(vpc_val *x);

vpc_val *vpcf_null(int n, vpc_val** xs);
vpc_val *vpcf_fst(int n, vpc_val** xs);
vpc_val *vpcf_snd(int n, vpc_val** xs);
vpc_val *vpcfrd(int n, vpc_val** xs);

vpc_val *vpcf_fst_free(int n, vpc_val** xs);
vpc_val *vpcf_snd_free(int n, vpc_val** xs);
vpc_val *vpcfrd_free(int n, vpc_val** xs);

vpc_val *vpcf_strfold(int n, vpc_val** xs);
vpc_val *vpcf_maths(int n, vpc_val** xs);

vpc_parser *vpc_re(const char *re);
  
vpc_ast *vpc_ast_new(const char *tag, const char *contents);
vpc_ast *vpc_ast_build(int n, const char *tag, ...);
vpc_ast *vpc_ast_add_root(vpc_ast *a);
vpc_ast *vpc_ast_add_child(vpc_ast *r, vpc_ast *a);
vpc_ast *vpc_ast_addag(vpc_ast *a, const char *t);
vpc_ast *vpc_astag(vpc_ast *a, const char *t);
vpc_ast *vpc_ast_state(vpc_ast *a, vpc_cur_state s);

void vpc_ast_delete(vpc_ast *a);
void vpc_ast_print(vpc_ast *a);
void vpc_ast_printo(vpc_ast *a, FILE *fp);

int vpc_ast_eq(vpc_ast *a, vpc_ast *b);

vpc_val *vpcf_fold_ast(int n, vpc_val **as);
vpc_val *vpcf_str_ast(vpc_val *c);
vpc_val *vpcf_state_ast(int n, vpc_val **xs);

vpc_parser *vpcaag(vpc_parser *a, const char *t);
vpc_parser *vpca_addag(vpc_parser *a, const char *t);
vpc_parser *vpca_root(vpc_parser *a);
vpc_parser *vpca_state(vpc_parser *a);
vpc_parser *vpcaotal(vpc_parser *a);

vpc_parser *vpca_not(vpc_parser *a);
vpc_parser *vpca_maybe(vpc_parser *a);

vpc_parser *vpca_many(vpc_parser *a);
vpc_parser *vpca_many1(vpc_parser *a);
vpc_parser *vpca_count(int n, vpc_parser *a);

vpc_parser *vpca_or(int n, ...);
vpc_parser *vpca_and(int n, ...);

vpc_parser *vpca_grammar(int flags, const char *grammar, ...);

vpc_err *vpca_lang(int flags, const char *language, ...);
vpc_err *vpca_lang_file(int flags, FILE *f, ...);
vpc_err *vpca_lang_pipe(int flags, FILE *f, ...);
vpc_err *vpca_lang_contents(int flags, const char *filename, ...);

void vpc_print(vpc_parser *p);

int vpc_test_pass(vpc_parser *p, const char *s, const void *d,
  int(*tester)(const void*, const void*), 
  vpc_dtor destructor, 
  void(*printer)(const void*));

int vpc_test_fail(vpc_parser *p, const char *s, const void *d,
  int(*tester)(const void*, const void*),
  vpc_dtor destructor,
  void(*printer)(const void*));
