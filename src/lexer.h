#ifndef _LEXER_H_
#define _LEXER_H_

#include <stdio.h>

#define COMMENT ';'

typedef struct token Token;
typedef struct stream Stream;
typedef struct lexer Lexer;

typedef enum
{
  TOK_LPAREN,
  TOK_RPAREN,
  TOK_STRING_LITERAL,
  TOK_INT_LITERAL,
  TOK_FLOAT_LITERAL,
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
    char *symbol;
    char *string;
    char *errmsg;
  };
};

struct stream
{
  enum
  {
    STREAM_FILE,
    STREAM_STRING,
  } type;
  union
  {
    FILE *file;
    struct
    {
      const char *data;
      size_t size;
      size_t pos;
    } string;
  } source;
  int (*getc_func) (struct stream *s);
  int (*ungetc_func) (int c, struct stream *s);
};

struct lexer
{
  Stream *stream;
  Token token;
  int line;
};

Stream *stream_file (FILE *file);
Stream *stream_string (const char *string, size_t size);
void stream_close (Stream *s);

char *lex_token_type (TokenType tt);
Lexer *lex_init ();
void lex_set_stream (Lexer *l, Stream *s);
Token lex_next (Lexer *lexer);
void lex_close (Lexer *lexer);

static inline int
sgetc (Stream *s)
{
  return s->getc_func (s);
}

static inline int
sungetc (int c, Stream *s)
{
  return s->ungetc_func (c, s);
}

#endif /* _LEXER_H_ */
