#include "lexer.h"
#include "error.h"
#include "lisp.h"
#include "port.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

//// helpers
static int eat_spacing_and_comments (Lexer *l, int firstchar);
static Token parse_number (Lexer *l, int firstchar);
static Token parse_string (Lexer *l);
static Token parse_char (Lexer *l);
static Token parse_comma (Lexer *l);
static Token try_parse_symbol (Lexer *l, int firstchar);

Lexer
lex_init (Lisp_Port *s)
{
  return (Lexer){
    .line = 1,
    .token = (Token){},
    .stream = s,
  };
}

Token
lex_next (Lexer *l)
{
  Token tok;
  int c;
  int line;

  c = port_read_char (l->stream);

  c = eat_spacing_and_comments (l, c);
  line = l->line;

  if (c == EOF)
    tok.type = TOK_EOF;
  else if (c == '(')
    tok.type = TOK_LPAREN;
  else if (c == ')')
    tok.type = TOK_RPAREN;
  else if (c == '\'')
    tok.type = TOK_QUOTE;
  else if (c == ',')
    tok = parse_comma (l);
  else if (c == '`')
    tok.type = TOK_QUASIQUOTE;
  else if (c == ')')
    tok.type = TOK_RPAREN;
  else if (isdigit (c))
    tok = parse_number (l, c);
  else if (c == '"')
    tok = parse_string (l);
  else if (c == '?')
    tok = parse_char (l);
  else
    tok = try_parse_symbol (l, c);

  tok.line = line;

  return tok;
}

static int
eat_spacing_and_comments (Lexer *l, int firstchar)
{
  if (firstchar == ';')
    {
      int c;
      while ((c = port_read_char (l->stream)) != EOF && c != '\n')
        ;
      return eat_spacing_and_comments (l, c);
    }

  if (firstchar == ' ' || firstchar == '\t')
    {
      return eat_spacing_and_comments (l, port_read_char (l->stream));
    }

  if (firstchar == '\n')
    {
      l->line++;
      return eat_spacing_and_comments (l, port_read_char (l->stream));
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
  if (buf == NULL)
    memory_error ();

  while (1)
    {
      if (c == '.')
        is_float = 1;
      if (pos + 1 >= bufsize) // +1 is for null termination
        {
          // reallocate the buffer doubling its size
          bufsize *= 2;
          char *newbuf = realloc (buf, bufsize);
          if (newbuf == NULL)
            {
              free (buf);
              memory_error ();
            }
          buf = newbuf;
        }

      buf[pos++] = c;

      c = port_peek_char (l->stream);
      if (c == EOF || !(isdigit (c) || c == '.'))
        break;
      port_read_char (l->stream);
    }

  buf[pos] = '\0';

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
  if (buf == NULL)
    memory_error ();

  while ((c = port_read_char (l->stream)) != EOF && c != '"' && c != '\n')
    {
      if (pos + 1 >= bufsize) // +1 is for null termination
        {
          // reallocate the buffer doubling its size
          bufsize *= 2;
          char *newbuf = realloc (buf, bufsize);
          if (newbuf == NULL)
            {
              free (buf);
              memory_error ();
            }
          buf = newbuf;
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
parse_char (Lexer *l)
{
  Token tok;
  int c = port_read_char (l->stream);

  tok.type = TOK_ERROR;
  if (c == '\\')
    {
      // escaped: ?\n ?\t ?\s are special, any other ?\x is just x,
      // e.g. ?\( ?\\ ?\"
      c = port_read_char (l->stream);
      switch (c)
        {
        case 'n':
          c = '\n';
          break;
        case 't':
          c = '\t';
          break;
        case 'r':
          c = '\r';
          break;
        case 's':
          c = ' ';
          break;
        case 'e':
          c = 27; // escape
          break;
        case '0':
          c = '\0';
          break;
        }
    }

  if (c == EOF)
    {
      tok.errmsg = "unterminated character literal";
      return tok;
    }
  if (c < 0 || c > 0x7F)
    {
      tok.errmsg = "non-ascii character literal";
      return tok;
    }

  // ?ab is not a character
  int next = port_peek_char (l->stream);
  if (next != EOF && !isspace (next) && next != '(' && next != ')'
      && next != '\'' && next != '"' && next != ';')
    {
      tok.errmsg = "invalid character literal";
      return tok;
    }

  tok.type = TOK_CHAR_LITERAL;
  tok.character = c;
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
  if (buf == NULL)
    memory_error ();

  while (1)
    {
      if (pos + 1 >= bufsize) // +1 is for null termination
        {
          // reallocate the buffer doubling its size
          bufsize *= 2;
          char *newbuf = realloc (buf, bufsize);
          if (newbuf == NULL)
            {
              free (buf);
              memory_error ();
            }
          buf = newbuf;
        }

      buf[pos++] = c;

      c = port_peek_char (l->stream);
      if (c == EOF || c == ' ' || c == '\n' || c == '(' || c == ')'
          || c == '\'')
        break;
      port_read_char (l->stream);
    }

  buf[pos] = '\0'; // null termination

  tok.type = TOK_SYMBOL;
  tok.symbol = buf;

  return tok;
}

static Token
parse_comma (Lexer *l)
{
  Token tok;
  int next = port_peek_char (l->stream);
  if (next == '@')
    {
    tok.type = TOK_SPLICE;
    port_read_char (l->stream);
    }
  else
    tok.type = TOK_UNQUOTE;
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
    case TOK_CHAR_LITERAL:
      return "CHAR_LITERAL";
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
