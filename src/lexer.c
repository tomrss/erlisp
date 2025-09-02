#include "lexer.h"
#include "lisp.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

//// helpers
static int eat_spacing_and_comments (Lexer *l, int firstchar);
static Token parse_number (Lexer *l, int firstchar);
static Token parse_string (Lexer *l);
static Token try_parse_symbol (Lexer *l, int firstchar);
static int stream_file_getc (Stream *s);
static int stream_file_ungetc (int c, Stream *s);
static int stream_string_getc (Stream *s);
static int stream_string_ungetc (int c, Stream *s);

Stream *
stream_file (FILE *file)
{
  Stream *s = malloc (sizeof (Stream));
  s->type = STREAM_FILE;
  s->source.file = file;
  s->getc_func = stream_file_getc;
  s->ungetc_func = stream_file_ungetc;
  return s;
}

Stream *
stream_string (const char *string, size_t size)
{
  Stream *s = malloc (sizeof (Stream));
  s->type = STREAM_STRING;
  s->source.string.data = string;
  s->source.string.size = size;
  s->source.string.pos = 0;
  s->getc_func = stream_string_getc;
  s->ungetc_func = stream_string_ungetc;
  return s;
}

void
stream_close (Stream *s)
{
  free (s);
}

Lexer *
lex_init ()
{
  Lexer *l;
  l = malloc (sizeof (Lexer));
  l->line = 1;
  l->token = (Token){};
  return l;
}

void
lex_set_stream (Lexer *l, Stream *s)
{
  l->stream = s;
  l->line = 1;
  l->token = (Token){};
}

Token
lex_next (Lexer *l)
{
  Token tok;
  int c;
  int line;

  c = sgetc (l->stream);

  c = eat_spacing_and_comments (l, c);
  line = l->line;

  if (c == EOF)
    {
      tok.type = TOK_EOF;
    }
  else if (c == '(')
    {
      tok.type = TOK_LPAREN;
    }
  else if (c == ')')
    {
      tok.type = TOK_RPAREN;
    }
  else if (c == '\'')
    {
      tok.type = TOK_QUOTE;
    }
  else if (c == ',')
    {
      int next = sgetc (l->stream);
      if (next == '@')
        {
          tok.type = TOK_SPLICE;
        }
      else
        {
          tok.type = TOK_UNQUOTE;
          sungetc (next, l->stream);
        }
    }
  else if (c == '`')
    {
      tok.type = TOK_QUASIQUOTE;
    }
  else if (c == ')')
    {
      tok.type = TOK_RPAREN;
    }
  else if (isdigit (c))
    {
      tok = parse_number (l, c);
    }
  else if (c == '"')
    {
      tok = parse_string (l);
    }
  else
    {
      tok = try_parse_symbol (l, c);
    }

  tok.line = line;

  return tok;
}

static int
eat_spacing_and_comments (Lexer *l, int firstchar)
{
  if (firstchar == ';')
    {
      int c;
      while ((c = sgetc (l->stream)) != EOF && c != '\n')
        ;
      return eat_spacing_and_comments (l, c);
    }

  if (firstchar == ' ' || firstchar == '\t')
    {
      return eat_spacing_and_comments (l, sgetc (l->stream));
    }

  if (firstchar == '\n')
    {
      l->line++;
      return eat_spacing_and_comments (l, sgetc (l->stream));
    }

  return firstchar;
}

static Token
parse_number (Lexer *l, int firstchar)
{
  Token tok;    // token to parse
  int is_float; // the number is floating point and not integer
  int c;        // char to read
  char *buf;    // buffer where the number is read (we pass it to atoi/atof)
  int bufsize;  // size of the buffer
  int pos;      // position in the string read

  c = firstchar;
  pos = 0;
  is_float = 0;
  bufsize = 20;
  buf = malloc (bufsize);

  do
    {
      if (c == '.')
        is_float = 1;
      if (pos + 1 >= bufsize) // +1 is for null termination
        {
          // reallocate the buffer doubling its size
          bufsize *= 2;
          buf = realloc (buf, bufsize);
          if (!buf)
            {
              // FIXME don't know what to do here...
              fprintf (stderr, "Out of memory!!\n");
            }
        }

      buf[pos++] = c;
    }
  while ((c = sgetc (l->stream)) != EOF && (isdigit (c) || c == '.'));

  buf[pos] = '\0';

  sungetc (c, l->stream);

  // TODO use strtol/strtod instead of atoi/atof
  if (is_float)
    {
      tok.type = TOK_FLOAT_LITERAL;
      tok.floating = atof (buf);
    }
  else // is integer
    {
      tok.type = TOK_INT_LITERAL;
      tok.integer = atoi (buf);
    }

  return tok;
}

static Token
parse_string (Lexer *l)
{
  Token tok;   // token to parse
  int c;       // char to read
  char *buf;   // buffer where the string is read
  int bufsize; // size of the buffer
  int pos;     // position in the string read

  pos = 0;
  bufsize = 20;
  buf = malloc (bufsize);

  while ((c = sgetc (l->stream)) != EOF && c != '"' && c != '\n')
    {
      if (pos + 1 >= bufsize) // +1 is for null termination
        {
          // reallocate the buffer doubling its size
          bufsize *= 2;
          buf = realloc (buf, bufsize);
          if (!buf)
            {
              // FIXME don't know what to do here...
              fprintf (stderr, "Out of memory!!\n");
            }
        }

      buf[pos++] = c;
    }

  buf[pos] = '\0'; // null termination

  if (c == '"') // string properly terminated, good
    {
      tok.type = TOK_STRING_LITERAL;
      tok.string = buf;
    }
  else // string unterminated, bad
    {
      tok.type = TOK_ERROR;
      tok.errmsg = "unterminated string literal";
    }

  return tok;
}

static Token
try_parse_symbol (Lexer *l, int firstchar)
{
  Token tok;   // token to parse
  int c;       // char to read
  char *buf;   // buffer where the string is read
  int bufsize; // size of the buffer
  int pos;     // position in the string read

  c = firstchar;
  pos = 0;
  bufsize = 20;
  buf = malloc (bufsize);

  do
    {
      if (pos + 1 >= bufsize) // +1 is for null termination
        {
          // reallocate the buffer doubling its size
          bufsize *= 2;
          buf = realloc (buf, bufsize);
          if (!buf)
            {
              // FIXME don't know what to do here...
              fprintf (stderr, "Out of memory!!\n");
            }
        }

      buf[pos++] = c;
    }
  while ((c = sgetc (l->stream)) != EOF && c != ' ' && c != '\n' && c != '('
         && c != ')' && c != '\'');

  buf[pos] = '\0'; // null termination

  sungetc (c, l->stream);

  tok.type = TOK_SYMBOL;
  tok.symbol = buf;

  return tok;
}

char *
lex_token_type (TokenType tt)
{
  switch (tt)
    {
    case TOK_LPAREN:
      return "LPAREN";
    case TOK_RPAREN:
      return "RPAREN";
    case TOK_STRING_LITERAL:
      return "STRING_LITERAL";
    case TOK_INT_LITERAL:
      return "INT_LITERAL";
    case TOK_FLOAT_LITERAL:
      return "FLOAT_LITERAL";
    case TOK_SYMBOL:
      return "SYMBOL";
    case TOK_QUOTE:
      return "QUOTE";
    case TOK_UNQUOTE:
      return "UNQUOTE";
    case TOK_QUASIQUOTE:
      return "QUASIQUOTE";
    case TOK_SPLICE:
      return "SPLICE";
    case TOK_EOF:
      return "EOF";
    case TOK_ERROR:
      return "ERROR";
    default:
      return NULL;
    }
}

static int
stream_file_getc (Stream *s)
{
  return getc (s->source.file);
}

static int
stream_file_ungetc (int c, Stream *s)
{
  return ungetc (c, s->source.file);
}

static int
stream_string_getc (Stream *s)
{
  if (s->source.string.pos >= s->source.string.size)
    return EOF;
  return s->source.string.data[s->source.string.pos++];
}

static int
stream_string_ungetc (UNUSED int _, Stream *s)
{
  if (s->source.string.pos < 1)
    return 0;
  return s->source.string.data[--s->source.string.pos];
}
