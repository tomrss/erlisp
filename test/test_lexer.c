#include "test_lexer.h"
#include "../src/lexer.h"
#include "test_lib.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CMP_DOUBLE_EPSILON 1e-8

// test cases
static TestResult test_lexer_mix ();
static TestResult test_lexer_defer_eval ();
static TestResult test_lexer_comments ();
static TestResult test_lexer_unterminated_str ();
static TestResult test_lexer_float ();
static TestResult test_lexer_char ();

static TestCase test_lexer_cases[] = {
  { .skip = 0, .name = "mix", .run = &test_lexer_mix },
  { .skip = 0, .name = "defer-eval", .run = &test_lexer_defer_eval },
  { .skip = 0, .name = "comments", .run = &test_lexer_comments },
  { .skip = 0, .name = "unterm-str", .run = &test_lexer_unterminated_str },
  { .skip = 0, .name = "float", .run = &test_lexer_float },
  { .skip = 0, .name = "char", .run = &test_lexer_char },
  {}, // terminator
};

// assertion and helpers
static TestResult assert_tok_simple (Token tok, TokenType expected);
static TestResult assert_tok_string_literal (Token tok, char *expected);
static TestResult assert_tok_int_literal (Token tok, int expected);
static TestResult assert_tok_float_literal (Token tok, double expected);
static TestResult assert_tok_symbol (Token tok, char *expected);
static TestResult assert_tok_error (Token tok, char *expected);
static TestResult assert_tok_char_literal (Token tok, int expected);

TestSuite *
test_suite_lexer ()
{
  return test_suite_init ("lexer", test_lexer_cases);
}

// test cases implementation

static TestResult
test_lexer_mix ()
{
  FILE *f = fopen ("test/assets/src-mix.tl", "r");
  Stream *s = stream_file (f);
  Lexer *l = lex_init (s);
  if (!l)
    return TEST_RESULT_FAIL ("test asset not found");

  Token tok;
  TestResult res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_LPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "define");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_LPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "myfunc");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "arg1");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_RPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_LPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "do");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "this");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_string_literal (tok, "and that");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_RPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_LPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "+");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "arg1");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_float_literal (tok, 1.123);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_int_literal (tok, 999);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_RPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_RPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_EOF);
  if (!res.success)
    return res;

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lexer_comments ()
{
  FILE *f = fopen ("test/assets/src-comments.tl", "r");
  Stream *s = stream_file (f);
  Lexer *l = lex_init (s);
  if (!l)
    return TEST_RESULT_FAIL ("test asset not found");

  Token tok;
  TestResult res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_LPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "format");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "#t");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_string_literal (tok, "hello world");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_RPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_EOF);
  if (!res.success)
    return res;

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lexer_defer_eval ()
{
  FILE *f = fopen ("test/assets/src-defer-eval.tl", "r");
  Stream *s = stream_file (f);
  Lexer *l = lex_init (s);
  if (!l)
    return TEST_RESULT_FAIL ("test asset not found");

  Token tok;
  TestResult res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_QUASIQUOTE);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_LPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_int_literal (tok, 10);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_int_literal (tok, 5);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_UNQUOTE);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_LPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "sqrt");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_int_literal (tok, 4);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_RPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_SPLICE);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_LPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "map");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "sqrt");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_QUOTE);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_LPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_int_literal (tok, 16);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_int_literal (tok, 9);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_RPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_RPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_int_literal (tok, 8);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_RPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_EOF);
  if (!res.success)
    return res;

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lexer_unterminated_str ()
{
  FILE *f = fopen ("test/assets/src-err-unterminated-string.tl", "r");
  Stream *s = stream_file (f);
  Lexer *l = lex_init (s);
  if (!l)
    return TEST_RESULT_FAIL ("test asset not found");

  Token tok;
  TestResult res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_LPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "concat");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_string_literal (tok, "a");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_error (tok, "unterminated string literal");
  if (!res.success)
    return res;
  if (tok.line != 4)
    {
      res = TEST_RESULT_FAIL ("expected error at line %d, got line %d", 4,
                              tok.line);
      return res;
    }

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lexer_float ()
{
  const char *src = "1.5 0.25 3. 42 (a . b)";
  Lexer *l = lex_init (stream_string (src, strlen (src)));
  Token tok;
  TestResult res;

  tok = lex_next (l);
  res = assert_tok_float_literal (tok, 1.5);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_float_literal (tok, 0.25);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_float_literal (tok, 3.0);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_int_literal (tok, 42);
  if (!res.success)
    return res;

  // a lone dot is the dotted pair separator, not a float
  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_LPAREN);
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "a");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, ".");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_symbol (tok, "b");
  if (!res.success)
    return res;

  tok = lex_next (l);
  res = assert_tok_simple (tok, TOK_RPAREN);
  if (!res.success)
    return res;

  lex_close (l);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lexer_char ()
{
  // ?a ?( ?\n ?\s ?\\ ?\( ?ab
  const char *src = "?a ?( ?\\n ?\\s ?\\\\ ?\\( ?ab";
  int expected[] = { 'a', '(', '\n', ' ', '\\', '(' };
  int n = sizeof (expected) / sizeof (expected[0]);
  Lexer *l = lex_init (stream_string (src, strlen (src)));
  Token tok;
  TestResult res;

  for (int i = 0; i < n; i++)
    {
      tok = lex_next (l);
      res = assert_tok_char_literal (tok, expected[i]);
      if (!res.success)
        return res;
    }

  tok = lex_next (l);
  res = assert_tok_error (tok, "invalid character literal");
  if (!res.success)
    return res;
  lex_close (l);

  // a lone ? at the end of input
  const char *unterm = "?";
  l = lex_init (stream_string (unterm, strlen (unterm)));
  tok = lex_next (l);
  res = assert_tok_error (tok, "unterminated character literal");
  if (!res.success)
    return res;
  lex_close (l);

  return TEST_RESULT_SUCCESS;
}

// assertion and helpers

static TestResult
assert_tok_simple (Token tok, TokenType expected)
{
  if (tok.type != expected)
    {
      return TEST_RESULT_FAIL ("line %d: expected '%s', got '%s'", tok.line,
                               lex_token_type (expected),
                               lex_token_type (tok.type));
    }

  return TEST_RESULT_SUCCESS;
}

static TestResult
assert_tok_string_literal (Token tok, char *expected)
{
  if (tok.type != TOK_STRING_LITERAL)
    {
      return TEST_RESULT_FAIL ("line %d: expected %s '%s', got %s", tok.line,
                               lex_token_type (TOK_STRING_LITERAL), expected,
                               lex_token_type (tok.type));
    }

  if (strcmp (expected, tok.string) != 0)
    {
      return TEST_RESULT_FAIL ("line %d: expected '%s', got '%s'", tok.line,
                               expected, tok.string);
    }

  return TEST_RESULT_SUCCESS;
}

static TestResult
assert_tok_int_literal (Token tok, int expected)
{
  if (tok.type != TOK_INT_LITERAL)
    {
      return TEST_RESULT_FAIL ("line %d: expected %s %d, got %s", tok.line,
                               lex_token_type (TOK_INT_LITERAL), expected,
                               lex_token_type (tok.type));
    }

  if (expected != tok.integer)
    {
      return TEST_RESULT_FAIL ("line %d: expected %d, got %ld", tok.line,
                               expected, tok.integer);
    }

  return TEST_RESULT_SUCCESS;
}

static TestResult
assert_tok_float_literal (Token tok, double expected)
{
  if (tok.type != TOK_FLOAT_LITERAL)
    {
      return TEST_RESULT_FAIL ("line %d: expected %s %f, got %s", tok.line,
                               lex_token_type (TOK_FLOAT_LITERAL), expected,
                               lex_token_type (tok.type));
    }

  if (fabs (expected - tok.floating) > CMP_DOUBLE_EPSILON)
    {
      return TEST_RESULT_FAIL ("line %d: expected %f, got %f", tok.line,
                               expected, tok.floating);
    }

  return TEST_RESULT_SUCCESS;
}

static TestResult
assert_tok_symbol (Token tok, char *expected)
{
  if (tok.type != TOK_SYMBOL)
    {
      return TEST_RESULT_FAIL ("line %d: expected %s '%s', got '%s'", tok.line,
                               lex_token_type (TOK_SYMBOL), expected,
                               lex_token_type (tok.type));
    }

  if (strcmp (expected, tok.symbol) != 0)
    {
      return TEST_RESULT_FAIL ("line %d: expected '%s', got '%s'", expected,
                               tok.symbol);
    }

  return TEST_RESULT_SUCCESS;
}

static TestResult
assert_tok_error (Token tok, char *expected)
{
  if (tok.type != TOK_ERROR)
    {
      return TEST_RESULT_FAIL ("line %d: expected %s '%s', got %s",
                               lex_token_type (TOK_ERROR), expected,
                               lex_token_type (tok.type));
    }

  if (strcmp (expected, tok.errmsg) != 0)
    {
      return TEST_RESULT_FAIL ("line %d: expected '%s', got '%s'", tok.line,
                               expected, tok.symbol);
    }

  return TEST_RESULT_SUCCESS;
}

static TestResult
assert_tok_char_literal (Token tok, int expected)
{
  if (tok.type != TOK_CHAR_LITERAL)
    {
      return TEST_RESULT_FAIL ("line %d: expected %s %d, got %s", tok.line,
                               lex_token_type (TOK_CHAR_LITERAL), expected,
                               lex_token_type (tok.type));
    }

  if (expected != tok.character)
    {
      return TEST_RESULT_FAIL ("line %d: expected char %d, got %d", tok.line,
                               expected, tok.character);
    }

  return TEST_RESULT_SUCCESS;
}
