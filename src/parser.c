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
    add_to->expected = (char**) realloc(add_to->expected, sizeof(char*) * (long unsigned int) add_to->expected_count);
    add_to->expected[add_to->expected_count-1] = (char*) malloc(strlen(expected) + 1);
    strcpy(add_to->expected[add_to->expected_count-1], expected);
}

static void vpc_err_clear_expected(vpc_err* clear_from, char* expected){
    unsigned int i;
    for(i = 0; i < clear_from->expected_count; i++)
        free(clear_from->expected[i]);
    
    clear_from->expected_count = 1;
    clear_from->expected = (char**) realloc(clear_from->expected, sizeof(char*) * (long unsigned int) clear_from->expected_count);
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

int vpc_parse_contents(const char* filename, vpc_parser* p, vpc_result* r){
  FILE* f = fopen(filename, "rb");
  int res;
  
  if(f == NULL){
    r->output = NULL;
    r->error = vpc_err_fail(filename, vpc_state_new(), "Unable to open file!");
    return 0;
  }
  
  res = vpc_parse_file(filename, f, p, r);
  fclose(f);
  return res;
}

#undef vpc_CONTINUE
#undef vpc_SUCCESS
#undef vpc_FAILURE
#undef vpc_PRIMATIVE
