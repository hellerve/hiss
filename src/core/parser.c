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
    unsigned int n; 
    vpc_fold f; 
    vpc_parser* x; 
    vpc_dtor dx; 
} vpc_pdata_repeat;

typedef struct{ 
    unsigned int n; 
    vpc_parser** xs; 
} vpc_pdata_or;

typedef struct{ 
    unsigned int n; 
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
  vpc_pdata_not not_op;
  vpc_pdata_repeat repeat;
  vpc_pdata_or or_op;
  vpc_pdata_and and_op;
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
  unsigned int parsers_count;
  unsigned int parsers_slots;
  vpc_parser** parsers;
  unsigned int* states;

  unsigned int results_count;
  unsigned int results_slots;
  vpc_result* results;
  int* returns;
  
  vpc_err* err;
  
} vpc_stack;

/*
 *  Type based macros
 */

#define VPC_CONTINUE(st, x){ vpc_stack_set_state(stk, st); if(!vpc_stack_pushp(stk, x)) break; continue; }
#define VPC_SUCCESS(x){ vpc_stack_popp(stk, &p, &st); if(!vpc_stack_pushr(stk, vpc_result_out(x), 1)) break; continue; }
#define VPC_FAILURE(x){ vpc_stack_popp(stk, &p, &st); if(!vpc_stack_pushr(stk, vpc_result_err(x), 0)) break; continue; }
#define VPC_PRIMATIVE(x, f){ if(f) VPC_SUCCESS(x) else VPC_FAILURE(vpc_err_fail(i->filename, i->state, "Incorrect Input"))}

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
    vpc_cur_state* r = (vpc_cur_state*) malloc(sizeof(vpc_cur_state));
    memcpy(r, &s, sizeof(vpc_cur_state));
    return r;
}

/*
 *  Error functions
 */

static vpc_err* vpc_err_new(const char* filename, vpc_cur_state s, const char* expected, char recieved){
    vpc_err* err = (vpc_err*) malloc(sizeof(vpc_err));
    err->filename = (char*) malloc(strlen(filename) + 1);
    strcpy(err->filename, filename);
    err->state = s,
    err->expected_count = 1;
    err->expected = (char**) malloc(sizeof(char*));
    err->expected[0] = (char*) malloc(strlen(expected) + 1);
    strcpy(err->expected[0], expected);
    err->failure = NULL;
    err->recieved = recieved;
    return err;
}

static vpc_err* vpc_err_fail(const char* filename, vpc_cur_state s, const char* failure){
    vpc_err* err = (vpc_err*) malloc(sizeof(vpc_err));
    err->filename = (char*) malloc(strlen(filename) + 1);
    strcpy(err->filename, filename);
    err->state = s;
    err->expected_count = 0;
    err->expected = NULL;
    err->failure = (char*) malloc(strlen(failure) + 1);
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
    add_to->expected = (char**) realloc(add_to->expected, sizeof(char*) * add_to->expected_count);
    add_to->expected[add_to->expected_count-1] = (char*) malloc(strlen(expected) + 1);
    strcpy(add_to->expected[add_to->expected_count-1], expected);
}

static void vpc_err_clear_expected(vpc_err* clear_from, char* expected){
    unsigned int i;
    for(i = 0; i < clear_from->expected_count; i++)
        free(clear_from->expected[i]);
    
    clear_from->expected_count = 1;
    clear_from->expected = (char**) realloc(clear_from->expected, sizeof(char*) * clear_from->expected_count);
    clear_from->expected[0] = (char*) malloc(strlen(expected) + 1);
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

static char char_unescape_buffer[4];

static const char *vpc_err_char_unescape(char c) {
    char_unescape_buffer[0] = '\'';
    char_unescape_buffer[1] = ' ';
    char_unescape_buffer[2] = '\'';
    char_unescape_buffer[3] = '\0';

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

static vpc_err *vpc_err_or(vpc_err** es, unsigned int n){
    unsigned int i, j;
    vpc_err *e = (vpc_err*) malloc(sizeof(vpc_err));
    e->state = vpc_state_invalid();
    e->expected_count = 0;
    e->expected = NULL;
    e->failure = NULL;
    e->filename = (char*) malloc(strlen(es[0]->filename)+1);
    strcpy(e->filename, es[0]->filename);

    for(i = 0; i < n; i++)
        if(es[i]->state.pos > e->state.pos) e->state = es[i]->state;

    for(i = 0; i < n; i++){
        if(es[i]->state.pos < e->state.pos) continue;

        if(es[i]->failure){
            e->failure = (char*) malloc(strlen(es[i]->failure)+1);
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

static vpc_err* vpc_err_repeat(vpc_err* e, const char* prefix){
    unsigned int i;
    char* expect = (char*) malloc(strlen(prefix) + 1);
    strcpy(expect, prefix);

    if(e->expected_count == 1){
        expect = (char*) realloc(expect, strlen(expect) + strlen(e->expected[0]) + 1);
        strcat(expect, e->expected[0]);
    }

    if(e->expected_count > 1){
        for(i = 0; i < e->expected_count-2; i++){
            expect = (char*) realloc(expect, strlen(expect) + strlen(e->expected[i]) + strlen(", ") + 1);
            strcat(expect, e->expected[i]);
            strcat(expect, ", ");
        }

        expect = (char*) realloc(expect, strlen(expect) + strlen(e->expected[e->expected_count-2]) + strlen(" or ") + 1);
        strcat(expect, e->expected[e->expected_count-2]);
        strcat(expect, " or ");
        expect = (char*) realloc(expect, strlen(expect) + strlen(e->expected[e->expected_count-1]) + 1);
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
    char* prefix = (char*) malloc(digits + strlen(" of ") + 1);
    
    sprintf(prefix, "%i of ", n);
    c = vpc_err_repeat(e, prefix);
    
    free(prefix);
    
    return c;
}

/*
 * Input functions
 */

static vpc_input* vpc_input_new_string(const char*filename, const char* string){
    vpc_input* v = (vpc_input*) malloc(sizeof(vpc_input));
    v->filename = (char*) malloc(strlen(filename) + 1);
    strcpy(v->filename, filename);
    v->type = VPC_INPUT_STRING;
    v->state = vpc_state_new();
    v->string = (char*) malloc(strlen(string) + 1);
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
    vpc_input* v = (vpc_input*) malloc(sizeof(vpc_input));
    v->filename = (char*) malloc(strlen(filename) + 1);
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
    vpc_input* v = (vpc_input*) malloc(sizeof(vpc_input));
    v->filename = (char*) malloc(strlen(filename) + 1);
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
    v->marks = (vpc_cur_state*) realloc(v->marks, sizeof(vpc_cur_state) * v->marks_count);
    v->lasts = (char*) realloc(v->lasts, sizeof(char) * v->marks_count);
    v->marks[v->marks_count-1] = v->state;
    v->lasts[v->marks_count-1] = v->last;
    if(v->type == VPC_INPUT_PIPE && v->marks_count == 1) v->buffer = (char*) calloc(1, 1);
}

static void vpc_input_unmark(vpc_input* v){
    if (v->backtrack != VPC_TRUE) return;
    v->marks_count--;
    v->marks = (vpc_cur_state*) realloc(v->marks, sizeof(vpc_cur_state) * v->marks_count);
    v->lasts = (char*) realloc(v->lasts, sizeof(char) * v->marks_count);
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
      
      if(v->buffer && vpc_input_buffer_in_range(v))
        break;
      else
        ungetc(c, v->file);

    default: break;
  }
  return 0;
}

static int vpc_input_success(vpc_input* v, char c, char **o){
  if (v->type == VPC_INPUT_PIPE &&
      v->buffer &&
      !vpc_input_buffer_in_range(v)){
    v->buffer = (char*) realloc(v->buffer, strlen(v->buffer) + 2);
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
    *o = (char*) malloc(2);
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
  
  *o = (char*) malloc(strlen(c) + 1);
  strcpy(*o, c);
  return VPC_TRUE;
}

static int vpc_input_anchor(vpc_input* v, int(*f)(char,char)){
  return f(v->last, vpc_input_peekc(v));
}

/*
 *  Stack functions
 */

static vpc_stack* vpc_stack_new(const char* filename){
  vpc_stack* s = (vpc_stack*) malloc(sizeof(vpc_stack));
  
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
 *  Stack parser functions
 */

static void vpc_stack_set_state(vpc_stack* s, unsigned int x){
  s->states[s->parsers_count-1] = x;
}

static int vpc_stack_parsers_reserve_more(vpc_stack* s){
  vpc_parser** check_realloc = NULL;
  unsigned int* check_realloc2 = NULL;
  if(s->parsers_count > s->parsers_slots){
    s->parsers_slots = (unsigned int) ceil((s->parsers_slots+1) * 1.5);
    check_realloc = (vpc_parser**) realloc(s->parsers, sizeof(vpc_parser*) * s->parsers_slots);
    if(!check_realloc) return VPC_FALSE;
    s->parsers = check_realloc;
    check_realloc = NULL;
    check_realloc2 = (unsigned int*) realloc(s->states, sizeof(int) * s->parsers_slots);
    if(!check_realloc2) return VPC_FALSE;
    s->states = check_realloc2;
    check_realloc2 = 0;
  }
  return VPC_TRUE;
}

static int vpc_stack_parsers_reserve_less(vpc_stack* s){
  vpc_parser** check_realloc = NULL;
  unsigned int* check_realloc2 = NULL;
  if(s->parsers_slots > pow(s->parsers_count+1, 1.5)){
    s->parsers_slots = (unsigned int) floor((s->parsers_slots-1) * (1.0/1.5));
    check_realloc = (vpc_parser**) realloc(s->parsers, sizeof(vpc_parser*) * s->parsers_slots);
    if(!check_realloc) return VPC_FALSE;
    s->parsers = check_realloc;
    check_realloc = NULL;
    check_realloc2 = (unsigned int*) realloc(s->states, sizeof(int) * s->parsers_slots);
    if(!check_realloc2) return VPC_FALSE;
    s->states = check_realloc2;
    check_realloc2 = 0;
  }
  return VPC_TRUE;
}

static int vpc_stack_pushp(vpc_stack* s, vpc_parser* p){
  s->parsers_count++;
  if(!vpc_stack_parsers_reserve_more(s)) return VPC_FALSE;
  s->parsers[s->parsers_count-1] = p;
  s->states[s->parsers_count-1] = 0;
  return VPC_TRUE;
}

static void vpc_stack_popp(vpc_stack* s, vpc_parser** p, unsigned int* st){
  *p = s->parsers[s->parsers_count-1];
  *st = s->states[s->parsers_count-1];
  s->parsers_count--;
  vpc_stack_parsers_reserve_less(s);
}

static void vpc_stack_peepp(vpc_stack* s, vpc_parser** p, unsigned int* st){
  *p = s->parsers[s->parsers_count-1];
  *st = s->states[s->parsers_count-1];
}

static int vpc_stack_empty(vpc_stack *s){
  return s->parsers_count == 0;
}

/*
 *  Stack result functions
 */

static vpc_result vpc_result_err(vpc_err* e){
  vpc_result r;
  r.error = e;
  return r;
}

static vpc_result vpc_result_out(vpc_val* x){
  vpc_result r;
  r.output = x;
  return r;
}

static int vpc_stack_results_reserve_more(vpc_stack* s){
  vpc_result* realloc_check;
  int* realloc_check2;
  if (s->results_count > s->results_slots){
    s->results_slots = (unsigned int) ceil((s->results_slots + 1) * 1.5);
    realloc_check = (vpc_result*) realloc(s->results, sizeof(vpc_result) * s->results_slots);
    if(!realloc_check) return VPC_FALSE;
    s->results = realloc_check;
    realloc_check = NULL;
    realloc_check2 = (int*) realloc(s->returns, sizeof(int) * s->results_slots);
    if(!realloc_check2) return VPC_FALSE;
    s->returns = realloc_check2;
    realloc_check2 = NULL;
  }
  return VPC_TRUE;
}

static int vpc_stack_results_reserve_less(vpc_stack* s){
  vpc_result* realloc_check;
  int* realloc_check2;
  if(s->results_slots > pow(s->results_count+1, 1.5)){
    s->results_slots = (unsigned int) floor((s->results_slots-1) * (1.0/1.5));
    realloc_check = (vpc_result*) realloc(s->results, sizeof(vpc_result) * s->results_slots);
    if(!realloc_check) return VPC_FALSE;
    s->results = realloc_check;
    realloc_check = NULL;
    realloc_check2 = (int*) realloc(s->returns, sizeof(int) * s->results_slots);
    if(!realloc_check2) return VPC_FALSE;
    s->returns = realloc_check2;
    realloc_check2 = NULL;
  }
  return VPC_TRUE;
}

static int vpc_stack_pushr(vpc_stack* s, vpc_result x, int r){
  s->results_count++;
  if(!vpc_stack_results_reserve_more(s)) return VPC_FALSE;
  s->results[s->results_count-1] = x;
  s->returns[s->results_count-1] = r;
  return VPC_TRUE;
}

static int vpc_stack_popr(vpc_stack* s, vpc_result* x){
  int r;
  *x = s->results[s->results_count-1];
  r = s->returns[s->results_count-1];
  s->results_count--;
  vpc_stack_results_reserve_less(s);
  return r;
}

static int vpc_stack_peekr(vpc_stack* s, vpc_result* x){
  *x = s->results[s->results_count-1];
  return s->returns[s->results_count-1];
}

static void vpc_stack_popr_err(vpc_stack* s, unsigned int n){
  vpc_result x;
  while(n){
    vpc_stack_popr(s, &x);
    vpc_stack_err(s, x.error);
    n--;
  }
}

static void vpc_stack_popr_out(vpc_stack* s, unsigned int n, vpc_dtor* ds){
  vpc_result x;
  while(n){
    vpc_stack_popr(s, &x);
    ds[n-1](x.output);
    n--;
  }
}

static void vpc_stack_popr_out_single(vpc_stack* s, unsigned int n, vpc_dtor dx){
  vpc_result x;
  while(n){
    vpc_stack_popr(s, &x);
    dx(x.output);
    n--;
  }
}

static void vpc_stack_popr_n(vpc_stack* s, unsigned int n){
  vpc_result x;
  while(n){
    vpc_stack_popr(s, &x);
    n--;
  }
}

static vpc_val* vpc_stack_merger_out(vpc_stack* s, unsigned int n, vpc_fold f){
  vpc_val* x = f(n, (vpc_val**)(&s->results[s->results_count-n]));
  vpc_stack_popr_n(s, n);
  return x;
}

static vpc_err* vpc_stack_merger_err(vpc_stack* s, unsigned int n){
  vpc_err* x = vpc_err_or((vpc_err**)(&s->results[s->results_count-n]), n);
  vpc_stack_popr_n(s, n);
  return x;
}

/*
 * Parser building functions
 */

static void vpc_undefine_unretained(vpc_parser* p, unsigned int force);

static void vpc_undefine_or(vpc_parser* p){
  unsigned int i;
  for(i = 0; i < p->data.or_op.n; i++)
    vpc_undefine_unretained(p->data.or_op.xs[i], 0);
  free(p->data.or_op.xs);
}

static void vpc_undefine_and(vpc_parser* p){
  unsigned int i;
  for(i = 0; i < p->data.and_op.n; i++)
    vpc_undefine_unretained(p->data.and_op.xs[i], 0);
  free(p->data.and_op.xs);
  free(p->data.and_op.dxs);
}

static void vpc_undefine_unretained(vpc_parser* p, unsigned int force){
  if(p->retained && !force) return;

  switch(p->type){
    case VPC_TYPE_FAIL: free(p->data.fail.m); break;
    case VPC_TYPE_ONEOF: 
    case VPC_TYPE_NONEOF:
    case VPC_TYPE_STRING:
      free(p->data.string.x); 
      break;
    case VPC_TYPE_APPLY:    vpc_undefine_unretained(p->data.apply.x, 0);    break;
    case VPC_TYPE_APPLY_TO: vpc_undefine_unretained(p->data.apply_to.x, 0); break;
    case VPC_TYPE_PREDICT:  vpc_undefine_unretained(p->data.predict.x, 0);  break;
    case VPC_TYPE_MAYBE:
    case VPC_TYPE_NOT:
      vpc_undefine_unretained(p->data.not_op.x, 0);
      break;
    case VPC_TYPE_EXPECT:
      vpc_undefine_unretained(p->data.expect.x, 0);
      free(p->data.expect.m);
      break;
    case VPC_TYPE_MANY:
    case VPC_TYPE_MANY1:
    case VPC_TYPE_COUNT:
      vpc_undefine_unretained(p->data.repeat.x, 0);
      break;
    case VPC_TYPE_OR:  vpc_undefine_or(p);  break;
    case VPC_TYPE_AND: vpc_undefine_and(p); break;
    default: break;
  }

  if (!force) {
    free(p->name);
    free(p);
  }
}

static void vpc_soft_delete(vpc_val* x){
  vpc_undefine_unretained(x, 0);
}

static vpc_parser* vpc_undefined(void){
  vpc_parser* p = calloc(1, sizeof(vpc_parser));
  p->retained = 0;
  p->type = VPC_TYPE_UNDEFINED;
  p->name = NULL;
  return p;
}

/*
 * Common Parser functions
 */

static int vpc_soi_anchor(char prev, char next){ 
    (void) next; 
    return (prev == '\0'); 
}

static int vpc_eoi_anchor(char prev, char next){ 
    (void) prev; 
    return (next == '\0'); 
}

static int vpc_boundary_anchor(char prev, char next){
  const char* word = "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                     "0123456789_";
  if(strchr(word, next) &&  prev == '\0') return 1;
  if(strchr(word, prev) &&  next == '\0') return 1;
  if(strchr(word, next) && !strchr(word, prev)) return 1;
  if(!strchr(word, next) &&  strchr(word, prev)) return 1;
  return 0;
}

/*
 * Regular Expression Parser functions
 */

/*
 * So here is a cute bootstrapping.
 *
 * I'm using the previously defined
 * vpc(sorry, dan) constructs and functions to
 * parse the user regex string and
 * construct a parser from it.
 *
 * As it turns out lots of the standard
 * vpc functions look a lot like `fold`
 * functions and so can be used indirectly
 * by many of the parsing functions to build
 * a parser directly - as we are parsing.
 *
 * This is certainly something that
 * would be less elegant/interesting 
 * in a two-phase parser which first
 * builds an AST and then traverses it
 * to generate the object.
 *
 * This whole thing acts as a great
 * case study for how trivial it can be
 * to write a great parser in a few
 * lines of code using vpc.
 */

/*
 *  ### Regular Expression Grammar
 *
 *      <regex> : <term> | (<term> "|" <regex>)
 *     
 *      <term> : <factor>*
 *
 *      <factor> : <base>
 *               | <base> "*"
 *               | <base> "+"
 *               | <base> "?"
 *               | <base> "{" <digits> "}"
 *           
 *      <base> : <char>
 *             | "\" <char>
 *             | "(" <regex> ")"
 *             | "[" <range> "]"
 */

static vpc_val* vpcf_re_or(unsigned int n, vpc_val** xs){
  (void) n;
  if(xs[1] == NULL) return xs[0];
  else return vpc_or(2, xs[0], xs[1]);
}

static vpc_val* vpcf_re_and(unsigned int n, vpc_val** xs){
  unsigned int i;
  vpc_parser* p = vpc_lift(vpcf_ctor_str);
  for(i = 0; i < n; i++){
    p = vpc_and(2, vpcf_strfold, p, xs[i], free);
  }
  return p;
}

static vpc_val* vpcf_re_repeat(unsigned int n, vpc_val** xs){
  unsigned int num;
  (void) n;
  if(xs[1] == NULL) return xs[0];
  if(strcmp(xs[1], "*") == 0){ free(xs[1]); return vpc_many(vpcf_strfold, xs[0]); }
  if(strcmp(xs[1], "+") == 0){ free(xs[1]); return vpc_many1(vpcf_strfold, xs[0]); }
  if(strcmp(xs[1], "?") == 0){ free(xs[1]); return vpc_maybe_lift(xs[0], vpcf_ctor_str); }
  num = *(unsigned int*)xs[1];
  free(xs[1]);

  return vpc_count(num, vpcf_strfold, xs[0], free);
}

static vpc_parser* vpc_re_escape_char(char c){
  switch(c){
    case 'a': return vpc_char('\a');
    case 'f': return vpc_char('\f');
    case 'n': return vpc_char('\n');
    case 'r': return vpc_char('\r');
    case 't': return vpc_char('\t');
    case 'v': return vpc_char('\v');
    case 'b': return vpc_and(2, vpcf_snd, vpc_boundary(), vpc_lift(vpcf_ctor_str), free);
    case 'B': return vpc_not_lift(vpc_boundary(), free, vpcf_ctor_str);
    case 'A': return vpc_and(2, vpcf_snd, vpc_soi(), vpc_lift(vpcf_ctor_str), free);
    case 'Z': return vpc_and(2, vpcf_snd, vpc_eoi(), vpc_lift(vpcf_ctor_str), free);
    case 'd': return vpc_digit();
    case 'D': return vpc_not_lift(vpc_digit(), free, vpcf_ctor_str);
    case 's': return vpc_whitespace();
    case 'S': return vpc_not_lift(vpc_whitespace(), free, vpcf_ctor_str);
    case 'w': return vpc_alphanum();
    case 'W': return vpc_not_lift(vpc_alphanum(), free, vpcf_ctor_str);
    default: return NULL;
  }
}

static vpc_val* vpcf_re_escape(vpc_val* x){
  char* s = x;
  vpc_parser* p;

  /* Regex Special Characters */
  if(s[0] == '.'){ free(s); return vpc_any(); }
  if(s[0] == '^'){ free(s); return vpc_and(2, vpcf_snd, vpc_soi(), vpc_lift(vpcf_ctor_str), free); }
  if(s[0] == '$'){ free(s); return vpc_and(2, vpcf_snd, vpc_eoi(), vpc_lift(vpcf_ctor_str), free); }

  /* Regex Escape */
  if(s[0] == '\\'){
    p = vpc_re_escape_char(s[1]);
    p = (p == NULL) ? vpc_char(s[1]) : p;
    free(s);
    return p;
  }

  /* Regex Standard */
  p = vpc_char(s[0]);
  free(s);
  return p;
}

static const char* vpc_re_range_escape_char(char c){
  switch(c){
    case '-': return "-";
    case 'a': return "\a";
    case 'f': return "\f";
    case 'n': return "\n";
    case 'r': return "\r";
    case 't': return "\t";
    case 'v': return "\v";
    case 'b': return "\b";
    case 'd': return "0123456789";
    case 's': return " \f\n\r\t\v";
    case 'w': return "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";
    default: return NULL;
  }
}

static vpc_val* vpcf_re_range(vpc_val* x){
  vpc_parser* out;
  char* range = calloc(1,1);
  const char* tmp = NULL;
  const char* s = x;
  size_t comp = s[0] == '^' ? 1 : 0;
  size_t start, end;
  size_t i, j;

  if(s[0] == '\0'){ free(x); return vpc_fail("Invalid Regex Range Expression"); } 
  if(s[0] == '^' && 
     s[1] == '\0'){ free(x); return vpc_fail("Invalid Regex Range Expression"); }

  for(i = comp; i < strlen(s); i++){
    /* Regex Range Escape */
    if (s[i] == '\\') {
      tmp = vpc_re_range_escape_char(s[i+1]);
      if(tmp){
        range = realloc(range, strlen(range) + strlen(tmp) + 1);
        strcat(range, tmp);
      } else {
        range = realloc(range, strlen(range) + 1 + 1);
        range[strlen(range) + 1] = '\0';
        range[strlen(range) + 0] = s[i+1];      
      }
      i++;
    }
    /* Regex Range...Range */
    else if(s[i] == '-'){
      if(s[i+1] == '\0' || i == 0){
          range = realloc(range, strlen(range) + strlen("-") + 1);
          strcat(range, "-");
      } else {
        start = (size_t) s[i-1]+1;
        end = (size_t) s[i+1]-1;
        for(j = start; j <= end; j++){
          range = realloc(range, strlen(range) + 1 + 1);
          range[strlen(range) + 1] = '\0';
          range[strlen(range) + 0] = (char) j;
        }        
      }
    }
    /* Regex Range Normal */
    else {
      range = realloc(range, strlen(range) + 1 + 1);
      range[strlen(range) + 1] = '\0';
      range[strlen(range) + 0] = s[i];
    }
  }

  out = comp == 1 ? vpc_noneof(range) : vpc_oneof(range);

  free(x);
  free(range);

  return out;
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
    char* buffer = (char*) calloc(1, 1024);
    int max = 1023;
    int pos = 0;
    unsigned int i;

    if(e->failure){
        vpc_err_string_cat(buffer, &pos, &max, "%s: error: %s\n", e->filename, e->failure);
        return buffer;
    }

    vpc_err_string_cat(buffer, &pos, &max, "%s:%i:%i: error: expected ", e->filename, 
                       e->state.row+1, e->state.col+1);

    if(e->expected_count == 0){ 
        vpc_err_string_cat(buffer, &pos, &max, "ERROR: NOTHING EXPECTED");
    } else if(e->expected_count == 1){
        vpc_err_string_cat(buffer, &pos, &max, "%s", e->expected[0]);
    } else if(e->expected_count >= 2){ 
        for(i = 0; i < e->expected_count-2; i++)
            vpc_err_string_cat(buffer, &pos, &max, "%s, ", e->expected[i]);

        vpc_err_string_cat(buffer, &pos, &max, "%s or %s",
                e->expected[e->expected_count-2],
                e->expected[e->expected_count-1]);
    }

    vpc_err_string_cat(buffer, &pos, &max, " at ");
    vpc_err_string_cat(buffer, &pos, &max, vpc_err_char_unescape(e->recieved));
    vpc_err_string_cat(buffer, &pos, &max, "\n");

    return (char*) realloc(buffer, strlen(buffer)+1);

}

/*
 * Parse function
 */


int vpc_parse_input(vpc_input* i, vpc_parser* init, vpc_result* final){
  unsigned int st = 0;
  vpc_parser *p = NULL;
  vpc_stack *stk = vpc_stack_new(i->filename);
  char *s;
  vpc_result r;

  /* Begin the madness */
  vpc_stack_pushp(stk, init);
  
  while(!vpc_stack_empty(stk)){
    vpc_stack_peepp(stk, &p, &st);
    switch (p->type) {
      /* Basic Parsers */
      case VPC_TYPE_ANY:       VPC_PRIMATIVE(s, vpc_input_any(i, &s))
      case VPC_TYPE_SINGLE:    VPC_PRIMATIVE(s, vpc_input_char(i, p->data.single.x, &s))
      case VPC_TYPE_RANGE:     VPC_PRIMATIVE(s, vpc_input_range(i, p->data.range.x, p->data.range.y, &s))
      case VPC_TYPE_ONEOF:     VPC_PRIMATIVE(s, vpc_input_oneof(i, p->data.string.x, &s))
      case VPC_TYPE_NONEOF:    VPC_PRIMATIVE(s, vpc_input_noneof(i, p->data.string.x, &s))
      case VPC_TYPE_SATISFY:   VPC_PRIMATIVE(s, vpc_input_satisfy(i, p->data.satisfy.f, &s))
      case VPC_TYPE_STRING:    VPC_PRIMATIVE(s, vpc_input_string(i, p->data.string.x, &s))
      
      /* Other parsers */
      case VPC_TYPE_UNDEFINED: VPC_FAILURE(vpc_err_fail(i->filename, i->state, "Parser Undefined!"))
      case VPC_TYPE_PASS:      VPC_SUCCESS(NULL);
      case VPC_TYPE_FAIL:      VPC_FAILURE(vpc_err_fail(i->filename, i->state, p->data.fail.m))
      case VPC_TYPE_LIFT:      VPC_SUCCESS(p->data.lift.lf());
      case VPC_TYPE_LIFT_VAL:  VPC_SUCCESS(p->data.lift.x);
      case VPC_TYPE_STATE:     VPC_SUCCESS(vpc_state_copy(i->state));
      case VPC_TYPE_ANCHOR:
        if(vpc_input_anchor(i, p->data.anchor.f)) VPC_SUCCESS(NULL)
        else VPC_FAILURE(vpc_err_new(i->filename, i->state, "anchor", vpc_input_peekc(i)))
      
      /* Application Parsers */
      case VPC_TYPE_EXPECT:
        if(st == VPC_FALSE) VPC_CONTINUE(1, p->data.expect.x)
        if(st == VPC_TRUE){
          if(vpc_stack_popr(stk, &r)){
            VPC_SUCCESS(r.output)
          }else {
            vpc_err_delete(r.error); 
            VPC_FAILURE(vpc_err_new(i->filename, i->state, p->data.expect.m, vpc_input_peekc(i)))
          }
        }
      case VPC_TYPE_APPLY:
        if(st == VPC_FALSE) VPC_CONTINUE(1, p->data.apply.x)
        if(st == VPC_TRUE){
          if(vpc_stack_popr(stk, &r))
            VPC_SUCCESS(p->data.apply.f(r.output))
          else
            VPC_FAILURE(r.error)
        }
      case VPC_TYPE_APPLY_TO:
        if(st == VPC_FALSE) VPC_CONTINUE(1, p->data.apply_to.x)
        if(st == VPC_TRUE){
          if(vpc_stack_popr(stk, &r))
            VPC_SUCCESS(p->data.apply_to.f(r.output, p->data.apply_to.d))
          else
            VPC_FAILURE(r.error)
        }
      case VPC_TYPE_PREDICT:
        if(st == VPC_FALSE){ 
            vpc_input_backtrack_disable(i); 
            VPC_CONTINUE(1, p->data.predict.x)
        }
        if(st == VPC_TRUE){
          vpc_input_backtrack_enable(i);
          vpc_stack_popp(stk, &p, &st);
          continue;
        }
      
      /* Optional Parsers */
      
      /* TODO: Update Not Error Message */
      
      case VPC_TYPE_NOT:
        if(st == VPC_FALSE){ 
            vpc_input_mark(i); 
            VPC_CONTINUE(1, p->data.not_op.x)
        }
        if(st == VPC_TRUE){
          if(vpc_stack_popr(stk, &r)){
            vpc_input_rewind(i);
            p->data.not_op.dx(r.output);
            VPC_FAILURE(vpc_err_new(i->filename, i->state, "opposite", vpc_input_peekc(i)))
          }else{
            vpc_input_unmark(i);
            vpc_stack_err(stk, r.error);
            VPC_SUCCESS(p->data.not_op.lf())
          }
        }
      case VPC_TYPE_MAYBE:
        if(st == VPC_FALSE) VPC_CONTINUE(1, p->data.not_op.x)
        if(st == VPC_TRUE){
          if(vpc_stack_popr(stk, &r)){
            VPC_SUCCESS(r.output)
          }else{
            vpc_stack_err(stk, r.error);
            VPC_SUCCESS(p->data.not_op.lf())
          }
        }
      
      /* Repeat Parsers */
      
      case VPC_TYPE_MANY:
        if(st == VPC_FALSE) VPC_CONTINUE(st+1, p->data.repeat.x)
        if(st >  0){
          if(vpc_stack_peekr(stk, &r)){
            VPC_CONTINUE(st+1, p->data.repeat.x)
          } else {
            vpc_stack_popr(stk, &r);
            vpc_stack_err(stk, r.error);
            VPC_SUCCESS(vpc_stack_merger_out(stk, st-1, p->data.repeat.f))
          }
        }
      case VPC_TYPE_MANY1:
        if(st == VPC_FALSE) VPC_CONTINUE(st+1, p->data.repeat.x)
        if(st >  0){
          if(vpc_stack_peekr(stk, &r)){
            VPC_CONTINUE(st+1, p->data.repeat.x)
          } else {
            if(st == 1){
              vpc_stack_popr(stk, &r);
              VPC_FAILURE(vpc_err_many1(r.error))
            } else {
              vpc_stack_popr(stk, &r);
              vpc_stack_err(stk, r.error);
              VPC_SUCCESS(vpc_stack_merger_out(stk, st-1, p->data.repeat.f))
            }
          }
        }
      case VPC_TYPE_COUNT:
        if(st == VPC_FALSE){ 
            vpc_input_mark(i); 
            VPC_CONTINUE(st+1, p->data.repeat.x)
        }
        if(st >  0){
          if(vpc_stack_peekr(stk, &r)){
            VPC_CONTINUE(st+1, p->data.repeat.x)
          } else {
            if(st != (p->data.repeat.n+1)){
              vpc_stack_popr(stk, &r);
              vpc_stack_popr_out_single(stk, st-1, p->data.repeat.dx);
              vpc_input_rewind(i);
              VPC_FAILURE(vpc_err_count(r.error, p->data.repeat.n))
            } else {
              vpc_stack_popr(stk, &r);
              vpc_stack_err(stk, r.error);
              vpc_input_unmark(i);
              VPC_SUCCESS(vpc_stack_merger_out(stk, st-1, p->data.repeat.f))
            }
          }
        }
        
      /* Combinatory Parsers */
      
      case VPC_TYPE_OR:
        if(p->data.or_op.n == 0) VPC_SUCCESS(NULL)
        if(st == 0) VPC_CONTINUE(st+1, p->data.or_op.xs[st])
        if(st <= p->data.or_op.n){
          if(vpc_stack_peekr(stk, &r)){
            vpc_stack_popr(stk, &r);
            vpc_stack_popr_err(stk, st-1);
            VPC_SUCCESS(r.output)
          }
          if(st <  p->data.or_op.n)VPC_CONTINUE(st+1, p->data.or_op.xs[st])
          if(st == p->data.or_op.n) VPC_FAILURE(vpc_stack_merger_err(stk, p->data.or_op.n))
        }
      case VPC_TYPE_AND:
        if(p->data.and_op.n == 0) VPC_SUCCESS(p->data.and_op.f(0, NULL))
        if (st == VPC_FALSE){ 
            vpc_input_mark(i); 
            VPC_CONTINUE(st+1, p->data.and_op.xs[st])
        }
        if(st <= p->data.and_op.n){
          if(!vpc_stack_peekr(stk, &r)){
            vpc_input_rewind(i);
            vpc_stack_popr(stk, &r);
            vpc_stack_popr_out(stk, st-1, p->data.and_op.dxs);
            VPC_FAILURE(r.error)
          }
          if(st <  p->data.and_op.n) VPC_CONTINUE(st+1, p->data.and_op.xs[st])
          if(st == p->data.and_op.n) {
              vpc_input_unmark(i); 
              VPC_SUCCESS(vpc_stack_merger_out(stk, p->data.and_op.n, p->data.and_op.f))
          }
        }
      
      default:
        VPC_FAILURE(vpc_err_fail(i->filename, i->state, "Unknown Parser Type ID!"))
    }
  }
  return vpc_stack_terminate(stk, final);
}

int vpc_parse(const char* filename, const char* string, vpc_parser* p, vpc_result* r){
  int x;
  vpc_input* i = vpc_input_new_string(filename, string);
  x = vpc_parse_input(i, p, r);
  vpc_input_delete(i);
  return x;
}

int vpc_parse_file(const char* filename, FILE* file, vpc_parser* p, vpc_result* r){
  int x;
  vpc_input* i = vpc_input_new_file(filename, file);
  x = vpc_parse_input(i, p, r);
  vpc_input_delete(i);
  return x;
}

int vpc_parse_pipe(const char* filename, FILE* pipe, vpc_parser* p, vpc_result* r){
  int x;
  vpc_input* i = vpc_input_new_pipe(filename, pipe);
  x = vpc_parse_input(i, p, r);
  vpc_input_delete(i);
  return x;
}

int vpc_parse_contents(char* filename, vpc_parser* p, vpc_result* r){
  char* tmp;
  FILE* f = fopen(filename, "rb");
  int res;
  
  if(f == NULL){
    /*WARNING: THIS IS HISS_SPECFIC!*/
    tmp = (char*) realloc(filename, strlen(filename) + strlen("/module.his"));
    if(tmp){
        filename = tmp;
        tmp = 0;
        f = fopen(strcat(filename, "/module.his"), "rb");
    }
    if(f == NULL){
        r->output = NULL;
        r->error = vpc_err_fail(filename, vpc_state_new(), "Unable to open file!");
        return 0;
    }
  }
  
  res = vpc_parse_file(filename, f, p, r);
  fclose(f);
  return res;
}

/*
 * Parser building functions
 */
void vpc_delete(vpc_parser* p){
  if(p->retained){
    if (p->type != VPC_TYPE_UNDEFINED) vpc_undefine_unretained(p, 0);

    free(p->name);
    free(p);
  } else {
    vpc_undefine_unretained(p, 0);  
  }
}

vpc_parser* vpc_new(const char* name){
  vpc_parser* p = vpc_undefined();
  p->retained = 1;
  p->name = (char*) realloc(p->name, strlen(name) + 1);
  strcpy(p->name, name);
  return p;
}

vpc_parser* vpc_undefine(vpc_parser* p){
  vpc_undefine_unretained(p, 1);
  p->type = VPC_TYPE_UNDEFINED;
  return p;
}

vpc_parser* vpc_define(vpc_parser* p, vpc_parser* a){
  if(p->retained){
    p->type = a->type;
    p->data = a->data;
  } else {
    vpc_parser* a2 = vpc_failf("Attempt to assign to Unretained Parser!");
    p->type = a2->type;
    p->data = a2->data;
    free(a2);
  }

  free(a);
  return p;  
}

void vpc_cleanup(unsigned int n, ...){
  unsigned int i;
  vpc_parser** list = malloc(sizeof(vpc_parser*) * n);
  va_list va;

  va_start(va, n);
  for(i = 0; i < n; i++) list[i] = va_arg(va, vpc_parser*);
  for(i = 0; i < n; i++) vpc_undefine(list[i]);
  for(i = 0; i < n; i++) vpc_delete(list[i]); 
  va_end(va);  

  free(list);
}

vpc_parser* vpc_pass(){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_PASS;
  return p;
}

vpc_parser* vpc_fail(const char* m){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_FAIL;
  p->data.fail.m = (char*) malloc(strlen(m) + 1);
  strcpy(p->data.fail.m, m);
  return p;
}

/*
 * As `snprintf` is not ANSI standard this 
 * function `vpc_failf` should be considered
 * unsafe.
 *
 * You have a few options if this is going to be
 * trouble.
 *
 * - Ensure the format string does not exceed
 *   the buffer length using precision specifiers
 *   such as `%.512s`.
 *
 * - Patch this function in your code base to 
 *   use `snprintf` or whatever variant your
 *   system supports.
 *
 * - Avoid it altogether.
 *
 */

vpc_parser* vpc_failf(const char* fmt, ...){
  va_list va;
  char* buffer;

  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_FAIL;

  va_start(va, fmt);
  buffer = malloc(2048);
  vsprintf(buffer, fmt, va);
  va_end(va);

  buffer = (char*) realloc(buffer, strlen(buffer) + 1);
  p->data.fail.m = buffer;
  return p;
}

vpc_parser* vpc_lift_val(vpc_val* x){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_LIFT_VAL;
  p->data.lift.x = x;
  return p;
}

vpc_parser* vpc_lift(vpc_ctor lf){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_LIFT;
  p->data.lift.lf = lf;
  return p;
}

vpc_parser* vpc_anchor(int(*f)(char,char)){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_ANCHOR;
  p->data.anchor.f = f;
  return p;
}

vpc_parser* vpc_state(){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_STATE;
  return p;
}

vpc_parser* vpc_expect(vpc_parser* a, const char* expected){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_EXPECT;
  p->data.expect.x = a;
  p->data.expect.m = (char*) malloc(strlen(expected) + 1);
  strcpy(p->data.expect.m, expected);
  return p;
}

/*
 * As `snprintf` is not ANSI standard this 
 * function `vpc_expectf` should be considered
 * unsafe.
 *
 * You have a few options if this is going to be
 * trouble.
 *
 * - Ensure the format string does not exceed
 *   the buffer length using precision specifiers
 *   such as `%.512s`.
 *
 * - Patch this function in your code base to 
 *   use `snprintf` or whatever variant your
 *   system supports.
 *
 * - Avoid it altogether.
 *
 */

vpc_parser* vpc_expectf(vpc_parser* a, const char* fmt, ...){
  va_list va;
  char* buffer;

  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_EXPECT;

  va_start(va, fmt);
  buffer = malloc(2048);
  vsprintf(buffer, fmt, va);
  va_end(va);

  buffer = realloc(buffer, strlen(buffer) + 1);
  p->data.expect.x = a;
  p->data.expect.m = buffer;
  return p;
}

/*
 * Basic parser functions
 */

vpc_parser* vpc_any(){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_ANY;
  return vpc_expect(p, "any character");
}

vpc_parser* vpc_char(char c){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_SINGLE;
  p->data.single.x = c;
  return vpc_expectf(p, "'%c'", c);
}

vpc_parser* vpc_range(char s, char e){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_RANGE;
  p->data.range.x = s;
  p->data.range.y = e;
  return vpc_expectf(p, "character between '%c' and '%c'", s, e);
}

vpc_parser* vpc_oneof(const char* s){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_ONEOF;
  p->data.string.x = malloc(strlen(s) + 1);
  strcpy(p->data.string.x, s);
  return vpc_expectf(p, "one of '%s'", s);
}

vpc_parser* vpc_noneof(const char* s){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_NONEOF;
  p->data.string.x = malloc(strlen(s) + 1);
  strcpy(p->data.string.x, s);
  return vpc_expectf(p, "one of '%s'", s);
}

vpc_parser* vpc_satisfy(int(*f)(char)){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_SATISFY;
  p->data.satisfy.f = f;
  return vpc_expectf(p, "character satisfying function %p", f);
}

vpc_parser* vpc_string(const char* s){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_STRING;
  p->data.string.x = malloc(strlen(s) + 1);
  strcpy(p->data.string.x, s);
  return vpc_expectf(p, "\"%s\"", s);
}

/*
 * Core parser functions
 */

vpc_parser* vpc_parse_apply(vpc_parser* a, vpc_apply f){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_APPLY;
  p->data.apply.x = a;
  p->data.apply.f = f;
  return p;
}

vpc_parser* vpc_parse_apply_to(vpc_parser* a, vpc_apply_to f, void *x){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_APPLY_TO;
  p->data.apply_to.x = a;
  p->data.apply_to.f = f;
  p->data.apply_to.d = x;
  return p;
}

vpc_parser* vpc_predictive(vpc_parser* a){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_PREDICT;
  p->data.predict.x = a;
  return p;
}

vpc_parser* vpc_not_lift(vpc_parser* a, vpc_dtor da, vpc_ctor lf){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_NOT;
  p->data.not_op.x = a;
  p->data.not_op.dx = da;
  p->data.not_op.lf = lf;
  return p;
}

vpc_parser* vpc_not(vpc_parser* a, vpc_dtor da){
  return vpc_not_lift(a, da, vpcf_ctor_null);
}

vpc_parser* vpc_maybe_lift(vpc_parser* a, vpc_ctor lf){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_MAYBE;
  p->data.not_op.x = a;
  p->data.not_op.lf = lf;
  return p;
}

vpc_parser* vpc_maybe(vpc_parser* a){
  return vpc_maybe_lift(a, vpcf_ctor_null);
}

vpc_parser* vpc_many(vpc_fold f, vpc_parser* a){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_MANY;
  p->data.repeat.x = a;
  p->data.repeat.f = f;
  return p;
}

vpc_parser* vpc_many1(vpc_fold f, vpc_parser* a){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_MANY1;
  p->data.repeat.x = a;
  p->data.repeat.f = f;
  return p;
}

vpc_parser* vpc_count(unsigned int n, vpc_fold f, vpc_parser* a, vpc_dtor da){
  vpc_parser* p = vpc_undefined();
  p->type = VPC_TYPE_COUNT;
  p->data.repeat.n = n;
  p->data.repeat.f = f;
  p->data.repeat.x = a;
  p->data.repeat.dx = da;
  return p;
}

vpc_parser* vpc_or(unsigned int n, ...){
  unsigned int i;
  va_list va;
  vpc_parser* p = vpc_undefined();

  p->type = VPC_TYPE_OR;
  p->data.or_op.n = n;
  p->data.or_op.xs = malloc(sizeof(vpc_parser*) * n);

  va_start(va, n);  
  for(i = 0; i < n; i++){
    p->data.or_op.xs[i] = va_arg(va, vpc_parser*);
  }
  va_end(va);

  return p;
}

vpc_parser* vpc_and(unsigned int n, vpc_fold f, ...){
  unsigned int i;
  va_list va;
  vpc_parser* p = vpc_undefined();

  p->type = VPC_TYPE_AND;
  p->data.and_op.n = n;
  p->data.and_op.f = f;
  p->data.and_op.xs = malloc(sizeof(vpc_parser*) * n);
  p->data.and_op.dxs = malloc(sizeof(vpc_dtor) * (n-1));

  va_start(va, f);  
  for(i = 0; i < n; i++) p->data.and_op.xs[i] = va_arg(va, vpc_parser*);
  for(i = 0; i < (n-1); i++) p->data.and_op.dxs[i] = va_arg(va, vpc_dtor);
  va_end(va);

  return p;
}

/*
 * Common Parser functions
 */

vpc_parser* vpc_soi(){ 
    return vpc_expect(vpc_anchor(vpc_soi_anchor), "start of input"); 
}

vpc_parser* vpc_eoi(){ 
    return vpc_expect(vpc_anchor(vpc_eoi_anchor), "end of input"); 
}


vpc_parser* vpc_boundary(){ 
    return vpc_expect(vpc_anchor(vpc_boundary_anchor), "boundary"); 
}

vpc_parser* vpc_whitespace(){ 
    return vpc_expect(vpc_oneof(" \f\n\r\t\v"), "whitespace"); 
}

vpc_parser* vpc_whitespaces(){ 
    return vpc_expect(vpc_many(vpcf_strfold, vpc_whitespace()), "spaces"); 
}

vpc_parser* vpc_blank(){ 
    return vpc_expect(vpc_parse_apply(vpc_whitespaces(), vpcf_free), "whitespace"); 
}

vpc_parser* vpc_newline(){ 
    return vpc_expect(vpc_char('\n'), "newline"); 
}

vpc_parser* vpc_tab(){ 
    return vpc_expect(vpc_char('\t'), "tab"); 
}

vpc_parser* vpc_escape(){ 
    return vpc_and(2, vpcf_strfold, vpc_char('\\'), vpc_any(), free); 
}

vpc_parser* vpc_digit(){ return vpc_expect(vpc_oneof("0123456789"), "digit"); }

vpc_parser* vpc_hexdigit(){ 
    return vpc_expect(vpc_oneof("0123456789ABCDEFabcdef"), "hex digit"); 
}

vpc_parser* vpc_octdigit(){ 
    return vpc_expect(vpc_oneof("01234567"), "oct digit"); 
}

vpc_parser* vpc_digits(){ 
    return vpc_expect(vpc_many1(vpcf_strfold, vpc_digit()), "digits"); 
}

vpc_parser* vpc_hexdigits(){ 
    return vpc_expect(vpc_many1(vpcf_strfold, vpc_hexdigit()), "hex digits"); 
}

vpc_parser* vpc_octdigits(){ 
    return vpc_expect(vpc_many1(vpcf_strfold, vpc_octdigit()), "oct digits"); 
}

vpc_parser* vpc_lower(){ 
    return vpc_expect(vpc_oneof("abcdefghijklmnopqrstuvwxyz"), "lowercase letter"); 
}

vpc_parser* vpc_upper(){ 
    return vpc_expect(vpc_oneof("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), "uppercase letter"); 
}
vpc_parser* vpc_alpha(){ 
    return vpc_expect(vpc_oneof("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"), "letter"); 
}

vpc_parser* vpc_underscore(){ return vpc_expect(vpc_char('_'), "underscore"); }

vpc_parser* vpc_alphanum(){ 
    return vpc_expect(vpc_or(3, vpc_alpha(), vpc_digit(), vpc_underscore()), "alphanumeric"); 
}

vpc_parser* vpc_int(){ 
    return vpc_expect(vpc_parse_apply(vpc_digits(), vpcf_int), "integer"); 
}

vpc_parser* vpc_hex(){ 
    return vpc_expect(vpc_parse_apply(vpc_hexdigits(), vpcf_hex), "hexadecimal"); 
}

vpc_parser* vpc_oct(){ 
    return vpc_expect(vpc_parse_apply(vpc_octdigits(), vpcf_oct), "octadecimal"); 
}

vpc_parser* vpc_number() 
    { return vpc_expect(vpc_or(3, vpc_int(), vpc_hex(), vpc_oct()), "number"); 
}
  
vpc_parser* vpc_real(){
  /* [+-]?\d+(\.\d+)?([eE][+-]?[0-9]+)? */
  vpc_parser *p0, *p1, *p2, *p30, *p31, *p32, *p3;

  p0 = vpc_maybe_lift(vpc_oneof("+-"), vpcf_ctor_str);
  p1 = vpc_digits();
  p2 = vpc_maybe_lift(vpc_and(2, vpcf_strfold, vpc_char('.'), vpc_digits(), free), vpcf_ctor_str);
  p30 = vpc_oneof("eE");
  p31 = vpc_maybe_lift(vpc_oneof("+-"), vpcf_ctor_str);
  p32 = vpc_digits();
  p3 = vpc_maybe_lift(vpc_and(3, vpcf_strfold, p30, p31, p32, free, free), vpcf_ctor_str);

  return vpc_expect(vpc_and(4, vpcf_strfold, p0, p1, p2, p3, free, free, free), "real");
}

vpc_parser* vpc_float(){
  return vpc_expect(vpc_parse_apply(vpc_real(), vpcf_float), "float");
}

vpc_parser* vpc_char_lit(){
  return vpc_expect(vpc_between(vpc_or(2, vpc_escape(), vpc_any()), free, "'", "'"), "char");
}

vpc_parser* vpc_string_lit(){
  vpc_parser* strchar = vpc_or(2, vpc_escape(), vpc_noneof("\""));
  return vpc_expect(vpc_between(vpc_many(vpcf_strfold, strchar), free, "\"", "\""), "string");
}

vpc_parser* vpc_regex_lit(){  
  vpc_parser* regexchar = vpc_or(2, vpc_escape(), vpc_noneof("/"));
  return vpc_expect(vpc_between(vpc_many(vpcf_strfold, regexchar), free, "/", "/"), "regex");
}

vpc_parser* vpc_ident(){
  vpc_parser *p0, *p1; 
  p0 = vpc_or(2, vpc_alpha(), vpc_underscore());
  p1 = vpc_many(vpcf_strfold, vpc_alphanum()); 
  return vpc_and(2, vpcf_strfold, p0, p1, free);
}

/*
 * Regular Expression Parser functions
 */

/*
 * So here is a cute bootstrapping.
 *
 * I'm using the previously defined
 * vpc(sorry, dan) constructs and functions to
 * parse the user regex string and
 * construct a parser from it.
 *
 * As it turns out lots of the standard
 * vpc functions look a lot like `fold`
 * functions and so can be used indirectly
 * by many of the parsing functions to build
 * a parser directly - as we are parsing.
 *
 * This is certainly something that
 * would be less elegant/interesting 
 * in a two-phase parser which first
 * builds an AST and then traverses it
 * to generate the object.
 *
 * This whole thing acts as a great
 * case study for how trivial it can be
 * to write a great parser in a few
 * lines of code using vpc.
 */

/*
 *  ### Regular Expression Grammar
 *
 *      <regex> : <term> | (<term> "|" <regex>)
 *     
 *      <term> : <factor>*
 *
 *      <factor> : <base>
 *               | <base> "*"
 *               | <base> "+"
 *               | <base> "?"
 *               | <base> "{" <digits> "}"
 *           
 *      <base> : <char>
 *             | "\" <char>
 *             | "(" <regex> ")"
 *             | "[" <range> "]"
 */
vpc_parser* vpc_re(const char *re){
  char *err_msg;
  vpc_parser* err_out;
  vpc_result r;
  vpc_parser *Regex, *Term, *Factor, *Base, *Range, *RegexEnclose; 

  Regex  = vpc_new("regex");
  Term   = vpc_new("term");
  Factor = vpc_new("factor");
  Base   = vpc_new("base");
  Range  = vpc_new("range");

  vpc_define(Regex, vpc_and(2, vpcf_re_or,
    Term, 
    vpc_maybe(vpc_and(2, vpcf_snd_free, vpc_char('|'), Regex, free)),
    (vpc_dtor)vpc_delete
  ));

  vpc_define(Term, vpc_many(vpcf_re_and, Factor));

  vpc_define(Factor, vpc_and(2, vpcf_re_repeat,
    Base,
    vpc_or(5,
      vpc_char('*'), vpc_char('+'), vpc_char('?'),
      vpc_brackets(vpc_int(), free),
      vpc_pass()),
    (vpc_dtor)vpc_delete
  ));

  vpc_define(Base, vpc_or(4,
    vpc_parens(Regex, (vpc_dtor)vpc_delete),
    vpc_squares(Range, (vpc_dtor)vpc_delete),
    vpc_parse_apply(vpc_escape(), vpcf_re_escape),
    vpc_parse_apply(vpc_noneof(")|"), vpcf_re_escape)
  ));

  vpc_define(Range, vpc_parse_apply(
    vpc_many(vpcf_strfold, vpc_or(2, vpc_escape(), vpc_noneof("]"))),
    vpcf_re_range
  ));

  RegexEnclose = vpc_whole(vpc_predictive(Regex), (vpc_dtor)vpc_delete);

  if(!vpc_parse("<vpc_re_compiler>", re, RegexEnclose, &r)) {
    err_msg = vpc_err_string(r.error);
    err_out = vpc_failf("Invalid Regex: %s", err_msg);
    vpc_err_delete(r.error);  
    free(err_msg);
    r.output = err_out;
  }

  vpc_delete(RegexEnclose);
  vpc_cleanup(5, Regex, Term, Factor, Base, Range);

  return r.output;
}

#undef VPC_CONTINUE
#undef VPC_SUCCESS
#undef VPC_FAILURE
#undef VPC_PRIMATIVE
