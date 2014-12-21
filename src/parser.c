#include "parser.h"

/*
 *  Types and Definitions
 */

/*
 *  Input type
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
    unsigned int marks_count;
    vpc_cur_state* marks;
    char* lasts;

    char last;
} vpc_input;

/*
 *  Parser type
 */

enum {
  VPC_TYPE_UNDEFINED = 0,
  VPC_TYPE_PASS      = 1,
  VPC_TYPE_FAIL      = 2,
  VPC_TYPE_LIFT      = 3,
  VPC_TYPE_LIFT_VAL  = 4,
  VPC_TYPE_EXPECT    = 5,
  VPC_TYPE_ANCHOR    = 6,
  VPC_TYPE_STATE     = 7,
  
  VPC_TYPE_ANY       = 8,
  VPC_TYPE_SINGLE    = 9,
  VPC_TYPE_ONEOF     = 10,
  VPC_TYPE_NONEOF    = 11,
  VPC_TYPE_RANGE     = 12,
  VPC_TYPE_SATISFY   = 13,
  VPC_TYPE_STRING    = 14,
  
  VPC_TYPE_APPLY     = 15,
  VPC_TYPE_APPLY_TO  = 16,
  VPC_TYPE_PREDICT   = 17,
  VPC_TYPE_NOT       = 18,
  VPC_TYPE_MAYBE     = 19,
  VPC_TYPE_MANY      = 20,
  VPC_TYPE_MANY1     = 21,
  VPC_TYPE_COUNT     = 22,
  
  VPC_TYPE_OR        = 23,
  VPC_TYPE_AND       = 24
};

typedef struct{ 
    char* m; 
} vpc_pdata_fail;

typedef struct{ 
    vpc_ctor lf; 
    void* x; 
} vpc_pdata_lift;

typedef struct{ 
    vpc_parser* x; 
    char* m; 
} vpc_pdata_expect;

typedef struct{ 
    int(*f)(char,char); 
} vpc_pdata_anchor;

typedef struct{ 
    char x; 
} vpc_pdata_single;

typedef struct{ 
    char x; 
    char y; 
} vpc_pdata_range;

typedef struct{ 
    int(*f)(char); 
} vpc_pdata_satisfy;

typedef struct{ 
    char* x; 
} vpc_pdata_string;

typedef struct{ 
    vpc_parser* x; 
    vpc_apply f; 
} vpc_pdata_apply;

typedef struct{ 
    vpc_parser* x; 
    vpc_apply_to f; 
    void* d; 
} vpc_pdata_apply_to;

typedef struct{ 
    vpc_parser* x; 
} vpc_pdata_predict;

typedef struct{ 
    vpc_parser* x; 
    vpc_dtor dx; 
    vpc_ctor lf; 
} vpc_pdata_not;

typedef struct{ 
    int n; 
    vpc_fold f; 
    vpc_parser* x; 
    vpc_dtor dx; 
} vpc_pdata_repeat;

typedef struct{ 
    int n; 
    vpc_parser** xs; 
} vpc_pdata_or;

typedef struct{ 
    int n; 
    vpc_fold f; 
    vpc_parser** xs; 
    vpc_dtor* dxs; 
} vpc_pdata_and;

typedef union{
  vpc_pdata_fail fail;
  vpc_pdata_lift lift;
  vpc_pdata_expect expect;
  vpc_pdata_anchor anchor;
  vpc_pdata_single single;
  vpc_pdata_range range;
  vpc_pdata_satisfy satisfy;
  vpc_pdata_string string;
  vpc_pdata_apply apply;
  vpc_pdata_apply_to apply_to;
  vpc_pdata_predict predict;
  vpc_pdata_not not;
  vpc_pdata_repeat repeat;
  vpc_pdata_or or;
  vpc_pdata_and and;
} vpc_pdata;

struct vpc_parser{
  char retained;
  char* name;
  char type;
  vpc_pdata data;
};

/*
** Stack Type
*/

typedef struct{
  int parsers_count;
  int parsers_slots;
  vpc_parser** parsers;
  int* states;

  int results_count;
  int results_slots;
  vpc_result* results;
  int* returns;
  
  vpc_err* err;
  
} vpc_stack;

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
 * Input functions
 */

static vpc_input* vpc_input_new_string(const char*filename, const char* string){
    vpc_input* v = malloc(sizeof(vpc_input));
    v->filename = malloc(strlen(filename) + 1);
    strcpy(v->filename, filename);
    v->type = VPC_INPUT_STRING;
    v->state = vpc_state_new();
    v->string = malloc(strlen(string) + 1);
    strcpy(v->string, string);
    v->buffer = NULL;
    v->file = NULL;
    v->backtrack = 1;
    v->marks_count = 0;
    v->marks = NULL;
    v->lasts = NULL;
    v->last = '\0';
    return v;
}


static vpc_input* vpc_input_new_pipe(const char* filename, FILE* pipe){
    vpc_input* v = malloc(sizeof(vpc_input));
    v->filename = malloc(strlen(filename) + 1);
    strcpy(v->filename, filename);
    v->type = VPC_INPUT_PIPE;
    v->state = vpc_state_new();
    v->string = NULL;
    v->buffer = NULL;
    if(pipe)
        v->file = pipe;
    else
        v->file = fopen(filename, "r");
    v->backtrack = 1;
    v->marks_count = 0;
    v->marks = NULL;
    v->lasts = NULL;
    v->last = '\0';
    return v;
}

static vpc_input* vpc_input_new_file(const char* filename, FILE* file){
    vpc_input* v = malloc(sizeof(vpc_input));
    v->filename = malloc(strlen(filename) + 1);
    strcpy(v->filename, filename);
    v->type = VPC_INPUT_FILE;
    v->state = vpc_state_new();
    v->string = NULL;
    v->buffer = NULL;
    if(file)
        v->file = file;
    else
        v->file = fopen(filename, "r");
    v->backtrack = 1;
    v->marks_count = 0;
    v->marks = NULL;
    v->lasts = NULL;
    v->last = '\0';
    return v;
}

static void vpc_input_delete(vpc_input* v){
    free(v->filename);
    if(v->type == VPC_INPUT_STRING) free(v->string);
    if(v->type == VPC_INPUT_PIPE) free(v->buffer);
    free(v->marks);
    free(v->lasts);
    free(v);
}

static void vpc_input_backtrack_disable(vpc_input* v) { 
    v->backtrack = VPC_FALSE; 
}

static void vpc_input_backtrack_enable(vpc_input* v){ 
    v->backtrack = VPC_TRUE; 
}

static void vpc_input_mark(vpc_input* v) {
    if(v->backtrack != VPC_TRUE) return;
    v->marks_count++;
    v->marks = realloc(v->marks, sizeof(vpc_cur_state) * v->marks_count);
    v->lasts = realloc(v->lasts, sizeof(char) * v->marks_count);
    v->marks[v->marks_count-1] = v->state;
    v->lasts[v->marks_count-1] = v->last;
    if(v->type == VPC_INPUT_PIPE && v->marks_count == 1) v->buffer = calloc(1, 1);
}

static void vpc_input_unmark(vpc_input* v){
    if (v->backtrack != VPC_TRUE) return;
    v->marks_count--;
    v->marks = realloc(v->marks, sizeof(vpc_cur_state) * v->marks_count);
    v->lasts = realloc(v->lasts, sizeof(char) * v->marks_count);
    if(v->type == VPC_INPUT_PIPE && v->marks_count == 0){
        free(v->buffer);
        v->buffer = NULL;
    }
}

static void vpc_input_rewind(vpc_input* v){
  if (v->backtrack < 1) return;
  
  v->state = v->marks[v->marks_count-1];
  v->last  = v->lasts[v->marks_count-1];
  
  if (v->type == VPC_INPUT_FILE)
    fseek(v->file, v->state.pos, SEEK_SET);
  
  vpc_input_unmark(v);
}

static int vpc_input_buffer_in_range(vpc_input* v){
  return v->state.pos < (long)(v->marks[0].pos + (long)strlen(v->buffer));
}

static char vpc_input_buffer_get(vpc_input* v){
  return v->buffer[v->state.pos - v->marks[0].pos];
}

static int vpc_input_terminated(vpc_input* v) {
  if(v->type == VPC_INPUT_STRING && v->state.pos == (long)strlen(v->string)) return VPC_TRUE;
  else if(v->type == VPC_INPUT_FILE && feof(v->file)) return VPC_TRUE;
  else if(v->type == VPC_INPUT_PIPE && feof(v->file)) return VPC_TRUE;
  return VPC_FALSE;
}

static char vpc_input_getc(vpc_input* v) {
  char c = '\0';
  
  switch(v->type){
    case VPC_INPUT_STRING: return v->string[v->state.pos];
    case VPC_INPUT_FILE: 
        c = (char) fgetc(v->file); 
        return c;
    case VPC_INPUT_PIPE:
      if(!v->buffer){ 
          c = (char) getc(v->file); 
          return c; 
      }
      
      if(v->buffer && vpc_input_buffer_in_range(v)){
        c = vpc_input_buffer_get(v);
        return c;
      }else{
        c = (char) getc(v->file);
        return c;
      }
    default: return c;
  }
}

static char vpc_input_peekc(vpc_input* v){  
  char c = '\0';
  
  switch (v->type){
    case VPC_INPUT_STRING: return v->string[v->state.pos];
    case VPC_INPUT_FILE: 
      c = (char) fgetc(v->file);
      if(feof(v->file)) return '\0';
      
      fseek(v->file, -1, SEEK_CUR);
      return c;
    case VPC_INPUT_PIPE:
      if(!v->buffer){
        c = (char) getc(v->file);
        if(feof(v->file)) return '\0'; 
        ungetc(c, v->file);
        return c;
      }
      
      if(v->buffer && vpc_input_buffer_in_range(v)){
        return vpc_input_buffer_get(v);
      }else{
        c = (char) getc(v->file);
        if(feof(v->file)) return '\0';
        ungetc(c, v->file);
        return c;
      }
    
    default: return c;
  }
}

static int vpc_input_failure(vpc_input* v, char c) {
  switch(v->type){
    case VPC_INPUT_STRING: break;
    case VPC_INPUT_FILE: 
        fseek(v->file, -1, SEEK_CUR);
        break;
    case VPC_INPUT_PIPE:
      if(!v->buffer){
          ungetc(c, v->file); 
          break;
      }
      
      if(v->buffer && vpc_input_buffer_in_range(v)){
        break;
      } else {
        ungetc(c, v->file);
      }
    default: break;
  }
  return 0;
}

static int vpc_input_success(vpc_input* v, char c, char **o){
  if (v->type == VPC_INPUT_PIPE &&
      v->buffer &&
      !vpc_input_buffer_in_range(v)){
    v->buffer = realloc(v->buffer, strlen(v->buffer) + 2);
    v->buffer[strlen(v->buffer) + 1] = '\0';
    v->buffer[strlen(v->buffer) + 0] = c;
  }
  
  v->last = c;
  v->state.pos++;
  v->state.col++;
  
  if(c == '\n'){
    v->state.col = 0;
    v->state.row++;
  }
  
  if(o){
    *o = malloc(2);
    *o[0] = c;
    *o[1] = '\0';
  }
  
  return VPC_TRUE;
}

static int vpc_input_any(vpc_input* v, char** o){
  char x = vpc_input_getc(v);
  if(vpc_input_terminated(v)) return 0;
  return vpc_input_success(v, x, o);
}

static int vpc_input_char(vpc_input* v, char c, char** o){
  char x = vpc_input_getc(v);
  if(vpc_input_terminated(v)) return 0;
  return x == c ? vpc_input_success(v, x, o) : vpc_input_failure(v, x);
}

static int vpc_input_range(vpc_input* v, char c, char d, char** o){
  char x = vpc_input_getc(v);
  if(vpc_input_terminated(v)) return 0;
  return x >= c && x <= d ? vpc_input_success(v, x, o) : vpc_input_failure(v, x);  
}

static int vpc_input_oneof(vpc_input* v, const char* c, char** o){
  char x = vpc_input_getc(v);
  if(vpc_input_terminated(v)) return 0;
  return strchr(c, x) != 0 ? vpc_input_success(v, x, o) : vpc_input_failure(v, x);  
}

static int vpc_input_noneof(vpc_input* v, const char* c, char** o){
  char x = vpc_input_getc(v);
  if(vpc_input_terminated(v)) return 0;
  return strchr(c, x) == 0 ? vpc_input_success(v, x, o) : vpc_input_failure(v, x);  
}

static int vpc_input_satisfy(vpc_input* v, int(*cond)(char), char** o){
  char x = vpc_input_getc(v);
  if(vpc_input_terminated(v)) return 0;
  return cond(x) ? vpc_input_success(v, x, o) : vpc_input_failure(v, x);  
}

static int vpc_input_string(vpc_input* v, const char* c, char** o){
  char* co = NULL;
  const char* x = c;

  vpc_input_mark(v);
  while(*x){
    if (vpc_input_char(v, *x, &co)){
      free(co);
    } else {
      vpc_input_rewind(v);
      return VPC_FALSE;
    }
    x++;
  }
  vpc_input_unmark(v);
  
  *o = malloc(strlen(c) + 1);
  strcpy(*o, c);
  return VPC_TRUE;
}

static int vpc_input_anchor(vpc_input* v, int(*f)(char,char)){
  return f(v->last, vpc_input_peekc(v));
}

/*
 *  Static functions
 */

static vpc_stack* vpc_stack_new(const char* filename){
  vpc_stack* s = malloc(sizeof(vpc_stack));
  
  s->parsers_count = 0;
  s->parsers_slots = 0;
  s->parsers = NULL;
  s->states = NULL;
  
  s->results_count = 0;
  s->results_slots = 0;
  s->results = NULL;
  s->returns = NULL;
  
  s->err = vpc_err_fail(filename, vpc_state_invalid(), "Unknown Error");
  
  return s;
}

static void vpc_stack_err(vpc_stack* s, vpc_err* e){
  vpc_err* errs[2];
  errs[0] = s->err;
  errs[1] = e;
  s->err = vpc_err_or(errs, 2);
}

static int vpc_stack_terminate(vpc_stack* s, vpc_result* r){
  int success = s->returns[0];
  
  if(success){
    r->output = s->results[0].output;
    vpc_err_delete(s->err);
  } else {
    vpc_stack_err(s, s->results[0].error);
    r->error = s->err;
  }
  
  free(s->parsers);
  free(s->states);
  free(s->results);
  free(s->returns);
  free(s);
  
  return success;
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

/*
 * Input functions
 */


