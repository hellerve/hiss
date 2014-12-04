#ifndef PARSER_H
#define PARSER_H

#define VP_LANG_DEFAULT 0

typedef struct {} vp_t;

typedef union {
  vp_err_t *error;
  vp_val_t *output;
} vp_result_t;

vp_t* vp_new(const char* name);

void vpa_lang(int mode, char* grammar, ...);

void vp_cleanup(int count, ...);

int vp_parse(const char* channel, const char* input, vp_t* vp, vp_result_t result);

void vp_ast_print(vp_val_t* ast);
void vp_ast_delete(vp_val_t* ast);

void vp_err_print(vp_err_t* ast);
void vp_err_delete(vp_err_t* ast);

#endif
