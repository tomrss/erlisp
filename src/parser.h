#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "lisp.h"

Lisp_Object parse_sexp (Lexer *l);
int parse_next_sexp (Lexer *l, Lisp_Object *out);

#endif /* PARSER_H */
