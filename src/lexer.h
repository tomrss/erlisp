#ifndef _LEXER_H_
#define _LEXER_H_

#include "lisp.h"
#include <stdio.h>

#define COMMENT ';'

typedef struct token Token;
typedef struct lexer Lexer;

typedef enum
{
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_STRING_LITERAL,
  TOK_INT_LITERAL,
  TOK_FLOAT_LITERAL,
  TOK_CHAR_LITERAL,
  TOK_SYMBOL,
  TOK_QUOTE,
  TOK_UNQUOTE,
  TOK_QUASIQUOTE,
  TOK_SPLICE,
  TOK_EOF,
  TOK_ERROR,
} TokenType;

// TODO: don't use null terminated strings, explicitely parse length
struct token
{
  TokenType type;
  int line;
  union
  {
    long int integer;
    double floating;
    unsigned char character; // TODO this not utf8
    char *symbol;
    char *string;
    char *errmsg;
  };
};

struct lexer
{
  Lisp_Port *stream;
  Token token;
  int line;
};

char *lex_token_type (TokenType tt);
Lexer lex_init (Lisp_Port *s);
Token lex_next (Lexer *lexer);

#endif /* _LEXER_H_ */
