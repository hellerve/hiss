#ifndef PARSER_H
#define PARSER_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {VPC_FALSE, VPC_TRUE};

/*
** State Type
*/

typedef struct {
  long pos;
  long row;
  long col;
} vpc_cur_state;

/*
** Error Type
*/

typedef struct {
  vpc_cur_state state;
  unsigned int expected_count;
  char *filename;
  char *failure;
  char **expected;
  char recieved;
} vpc_err;

void vpc_err_delete(vpc_err *e);
char *vpc_err_string(vpc_err *e);
void vpc_err_print(vpc_err *e);
void vpc_err_fprint(vpc_err *e, FILE *f);

/*
** Parsing
*/

typedef void vpc_val;

typedef union {
  vpc_err *error;
  vpc_val *output;
} vpc_result;

struct vpc_parser;
typedef struct vpc_parser vpc_parser;

int vpc_parse(const char *filename, const char *string, vpc_parser *p, vpc_result *r);
int vpc_parse_file(const char *filename, FILE *file, vpc_parser *p, vpc_result *r);
int vpc_parse_pipe(const char *filename, FILE *pipe, vpc_parser *p, vpc_result *r);
int vpc_parse_contents(const char *filename, vpc_parser *p, vpc_result *r);

/*
** Function Types
*/

typedef void(*vpc_dtor)(vpc_val*);
typedef vpc_val*(*vpc_ctor)(void);

typedef vpc_val*(*vpc_apply)(vpc_val*);
typedef vpc_val*(*vpc_apply_to)(vpc_val*,void*);
typedef vpc_val*(*vpc_fold)(unsigned int,vpc_val**);

/*
** Building a Parser
*/

vpc_parser *vpc_new(const char *name);
vpc_parser *vpc_define(vpc_parser *p, vpc_parser *a);
vpc_parser *vpc_undefine(vpc_parser *p);

void vpc_delete(vpc_parser *p);
void vpc_cleanup(int n, ...);

/*
** Basic Parsers
*/

vpc_parser *vpc_any(void);
vpc_parser *vpc_char(char c);
vpc_parser *vpc_range(char s, char e);
vpc_parser *vpc_oneof(const char *s);
vpc_parser *vpc_noneof(const char *s);
vpc_parser *vpc_satisfy(int(*f)(char));
vpc_parser *vpc_string(const char *s);

/*
** Other Parsers
*/

vpc_parser *vpc_pass(void);
vpc_parser *vpc_fail(const char *m);
vpc_parser *vpc_failf(const char *fmt, ...);
vpc_parser *vpc_lift(vpc_ctor f);
vpc_parser *vpc_lift_val(vpc_val *x);
vpc_parser *vpc_anchor(int(*f)(char,char));
vpc_parser *vpc_state(void);

/*
** Combinator Parsers
*/

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

/*
** Common Parsers
*/

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

/*
** Useful Parsers
*/

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

/*
** Common Function Parameters
*/

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

/*
** Regular Expression Parsers
*/

vpc_parser *vpc_re(const char *re);
  
/*
** AST
*/

typedef struct vpc_ast {
  char *tag;
  char *contents;
  vpc_cur_state state;
  unsigned int children_num;
  struct vpc_ast** children;
} vpc_ast;

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

/*
** Warning: This function currently doesn't test for equality of the `state` member!
*/
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

enum {
  VPCA_LANG_DEFAULT              = 0,
  VPCA_LANG_PREDICTIVE           = 1,
  VPCA_LANG_WHITESPACE_SENSITIVE = 2
};

vpc_parser *vpca_grammar(int flags, const char *grammar, ...);

vpc_err *vpca_lang(int flags, const char *language, ...);
vpc_err *vpca_lang_file(int flags, FILE *f, ...);
vpc_err *vpca_lang_pipe(int flags, FILE *f, ...);
vpc_err *vpca_lang_contents(int flags, const char *filename, ...);

/*
** Debug & Testing
*/

void vpc_print(vpc_parser *p);

int vpc_test_pass(vpc_parser *p, const char *s, const void *d,
  int(*tester)(const void*, const void*), 
  vpc_dtor destructor, 
  void(*printer)(const void*));

int vpc_test_fail(vpc_parser *p, const char *s, const void *d,
  int(*tester)(const void*, const void*),
  vpc_dtor destructor,
  void(*printer)(const void*));

#ifdef __cplusplus
}
#endif

#endif
