#include "parser.h"
#include "alloc.h"
#include "lexer.h"
#include "lisp.h"
#include "obarray.h"
#include <string.h>

// Helper to parse a Lisp form starting from a given token
static Lisp_Object parse_sexp_from_tok (Lexer *l, Token tok);

static void
parser_error (const char *msg, const Token *tok)
{
  fprintf (stderr, "Parse error at line %d: %s (token type %s)\n",
           tok ? tok->line : -1, msg,
           tok ? lex_token_type (tok->type) : "<none>");
  exit (1);
}

static Lisp_Object
parse_list (Lexer *l)
{
  Token tok = lex_next (l);

  if (tok.type == TOK_RPAREN)
    return q_nil;
  if (tok.type == TOK_EOF)
    parser_error ("Unexpected EOF in list", &tok);

  if (tok.type == TOK_SYMBOL && tok.symbol && strcmp (tok.symbol, ".") == 0)
    {
      Lisp_Object tail = parse_sexp (l);
      tok = lex_next (l);
      if (tok.type != TOK_RPAREN)
        parser_error ("Expected ')' after dotted pair", &tok);
      return tail;
    }

  Lisp_Object car = parse_sexp_from_tok (l, tok);
  Lisp_Object cdr = parse_list (l);
  return f_cons (car, cdr);
}

static Lisp_Object
parse_sexp_from_tok (Lexer *l, Token tok)
{
  switch (tok.type)
    {
    case TOK_LPAREN:
      return parse_list (l);
    case TOK_QUOTE:
      return f_cons (make_str_symbol ("quote"),
                     f_cons (parse_sexp (l), q_nil));
    case TOK_RPAREN:
      parser_error ("Unexpected ')'", &tok);
      return q_nil;
    case TOK_INT_LITERAL:
      return box_int (tok.integer);
    case TOK_STRING_LITERAL:
      // TODO lexer could get us the len too instead of null terminated string
      return make_string (tok.string);
    case TOK_SYMBOL:
      {
        // TODO this seems reeaaaally wrong

        // fake string for lookup in obarray. we don't want to lisp
        // alloc it, it should not be managed and gc'd.
        int len = strlen (tok.string);
        Lisp_String *s = malloc (len + sizeof (Lisp_String));
        strncpy (s->data, tok.string, len);
        s->size = len;
        Lisp_Object obsym = obarray_lookup_name (v_obarray, box_string(s));
        Lisp_Object ret;
        if (type_of (obsym) == LISP_SYMB)
          // return the symbol found
          ret = obsym;
        else
          // create new symbol
          ret = make_str_symbol (tok.symbol);
        // cleanup
        free(s);
        return ret;
      }
    case TOK_EOF:
      return q_nil;
    default:
      parser_error ("Unexpected token", &tok);
      return q_nil;
    }
}

Lisp_Object
parse_sexp (Lexer *l)
{
  return parse_sexp_from_tok (l, lex_next (l));
}

Lisp_Object
parse (Lexer *l)
{
  // Build up a list of forms
  Lisp_Object forms = q_nil;
  Lisp_Object tail = q_nil;

  Token tok;
  while ((tok = lex_next (l)).type != TOK_EOF)
    {
      Lisp_Object form = parse_sexp_from_tok (l, tok);
      Lisp_Object cell = make_cons (form, q_nil);

      if (eq (forms, q_nil))
        forms = cell;
      else
        f_setcdr (tail, cell);

      tail = cell;
    }

  // wrap in (progn ...)
  return f_cons (make_nstr_symbol ("progn", 5), forms);
}
