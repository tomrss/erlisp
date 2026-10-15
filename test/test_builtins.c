#include "../src/alloc.h"
#include "../src/env.h"
#include "../src/eval.h"
#include "../src/lexer.h"
#include "../src/lisp.h"
#include "../src/obarray.h"
#include "../src/parser.h"
#include "test_lib.h"
#include <stdio.h>
#include <string.h>

// helpers

static Lisp_Object parse_src (const char *src);
static int eval_src (const char *src, Lisp_Object *out);
static int same_form (Lisp_Object x, Lisp_Object y);
static const char *symname (Lisp_Object symbol);
// (test-catch form): evaluates form under a handler, returns caught on error
static void defsubr_test_catch ();

#define EXPECT_NO_ERROR(src, res)                                             \
  TEST_ASSERT (eval_src (src, &res), "%s: unexpected %s", src,                \
               symname (f_error_symbol (res)))

// for forms evaluated only for their side effects, e.g. a define
#define EXPECT_NO_ERROR_SRC(src)                                              \
  do                                                                          \
    {                                                                         \
      Lisp_Object _res;                                                       \
      EXPECT_NO_ERROR (src, _res);                                            \
    }                                                                         \
  while (0)

#define EXPECT_INT(src, expected)                                           \
  do                                                                          \
    {                                                                         \
      Lisp_Object _res;                                                       \
      EXPECT_NO_ERROR (src, _res);                                            \
      TEST_CHECK_TYPE (src, _res, LISP_INTG);                                 \
      TEST_ASSERT (unbox_int (_res) == (expected), "%s: expected %ld, got %ld", \
                   src, (long)(expected), (long)unbox_int (_res));            \
    }                                                                         \
  while (0)

// exact comparison: use values representable in a float (e.g. 0.5, 0.25)
#define EXPECT_FLOAT(src, expected)                                           \
  do                                                                          \
    {                                                                         \
      Lisp_Object _res;                                                       \
      EXPECT_NO_ERROR (src, _res);                                            \
      TEST_CHECK_TYPE (src, _res, LISP_FLOT);                                 \
      TEST_ASSERT (unbox_float (_res) == (Lisp_Float)(expected),              \
                   "%s: expected %f, got %f", src, (double)(expected),        \
                   (double)unbox_float (_res));                               \
    }                                                                         \
  while (0)

// expected is parsed but not evaluated
#define EXPECT_FORM(src, expected)                                            \
  do                                                                          \
    {                                                                         \
      Lisp_Object _res;                                                       \
      EXPECT_NO_ERROR (src, _res);                                            \
      Lisp_Object _exp = parse_src (expected);                                \
      TEST_ASSERT (same_form (_res, _exp), "%s: expected %s, got a %s", src,  \
                   expected, type_name (type_of (_res)));                     \
    }                                                                         \
  while (0)

#define EXPECT_ERROR(src, errsym)                                             \
  do                                                                          \
    {                                                                         \
      Lisp_Object _res;                                                       \
      TEST_ASSERT (!eval_src (src, &_res), "%s: expected %s, got no error",   \
                   src, symname (errsym));                                    \
      TEST_ASSERT (eq (f_error_symbol (_res), errsym),                        \
                   "%s: expected %s, got %s", src, symname (errsym),          \
                   symname (f_error_symbol (_res)));                          \
    }                                                                         \
  while (0)

// test cases
static TestResult test_builtins_car_cdr ();
static TestResult test_builtins_cadr ();
static TestResult test_builtins_cddr ();
static TestResult test_builtins_setcar_setcdr ();
static TestResult test_builtins_length ();
static TestResult test_builtins_assoc ();
static TestResult test_builtins_assq ();
static TestResult test_builtins_rassoc ();
static TestResult test_builtins_rassq ();
static TestResult test_builtins_eq ();
static TestResult test_builtins_equal ();
static TestResult test_builtins_eqv ();
static TestResult test_builtins_symbol ();
static TestResult test_builtins_symbol_value ();
static TestResult test_builtins_vector ();
static TestResult test_builtins_eval ();
static TestResult test_builtins_string_equal ();
static TestResult test_builtins_concat ();
static TestResult test_builtins_number_to_string ();
static TestResult test_builtins_number_to_string_edge ();
static TestResult test_builtins_string_to_number ();
static TestResult test_builtins_format ();
static TestResult test_builtins_sum ();
static TestResult test_builtins_subtract ();
static TestResult test_builtins_multiply ();
static TestResult test_builtins_divide ();
static TestResult test_builtins_compare ();
static TestResult test_builtins_num_eq ();
static TestResult test_builtins_min_max ();
static TestResult test_builtins_abs ();
static TestResult test_builtins_and ();
static TestResult test_builtins_or ();
static TestResult test_builtins_if ();
static TestResult test_builtins_when_unless ();
static TestResult test_builtins_cond ();
static TestResult test_builtins_let ();
static TestResult test_builtins_define_recursion ();
static TestResult test_builtins_define_noleak ();
static TestResult test_builtins_define_in_if ();
static TestResult test_builtins_define_shadow_arg ();
static TestResult test_builtins_define_per_call ();
static TestResult test_builtins_define_late ();
static TestResult test_builtins_define_child_snapshot ();
static TestResult test_builtins_define_global ();
static TestResult test_builtins_define_short ();
static TestResult test_builtins_define_short_error ();
static TestResult test_builtins_load_global ();
static TestResult test_builtins_load_error ();
static TestResult test_builtins_read ();
static TestResult test_builtins_port_p ();
static TestResult test_builtins_eof_object ();
static TestResult test_builtins_input_string_port ();
static TestResult test_builtins_peek_char ();
static TestResult test_builtins_output_string_port ();
static TestResult test_builtins_output_string_grow ();
static TestResult test_builtins_port_direction ();
static TestResult test_builtins_close_port ();
static TestResult test_builtins_port_type_errors ();
static TestResult test_builtins_file_port ();
static TestResult test_builtins_current_port ();
static TestResult test_builtins_port_gc ();
static TestResult test_builtins_unwind_protect ();
static TestResult test_builtins_unwind_protect_error ();
static TestResult test_builtins_unwind_protect_handler ();
static TestResult test_builtins_unwind_protect_cleanup_error ();
static TestResult test_builtins_unwind_protect_gc ();
static TestResult test_builtins_condition_case ();
static TestResult test_builtins_condition_case_rethrow ();
static TestResult test_builtins_condition_case_unwind ();
static TestResult test_builtins_condition_case_handlers ();
static TestResult test_builtins_condition_case_gc ();
static TestResult test_builtins_append ();
static TestResult test_builtins_defmacro ();
static TestResult test_builtins_defmacro_error ();
static TestResult test_builtins_quasiquote ();
static TestResult test_builtins_quasiquote_macro ();
static TestResult test_builtins_lambda_rest ();
static TestResult test_builtins_define_rest ();
static TestResult test_builtins_defmacro_rest ();
static TestResult test_builtins_lambda_rest_error ();

static TestCase test_builtins_cases[] = {
  { .skip = 0, .name = "car cdr", .run = test_builtins_car_cdr },
  { .skip = 0, .name = "cadr", .run = test_builtins_cadr },
  { .skip = 0, .name = "cddr", .run = test_builtins_cddr },
  { .skip = 0, .name = "setcar setcdr", .run = test_builtins_setcar_setcdr },
  { .skip = 0, .name = "length", .run = test_builtins_length },
  { .skip = 0, .name = "assoc", .run = test_builtins_assoc },
  { .skip = 0, .name = "assq", .run = test_builtins_assq },
  { .skip = 0, .name = "rassoc", .run = test_builtins_rassoc },
  { .skip = 0, .name = "rassq", .run = test_builtins_rassq },
  { .skip = 0, .name = "eq?", .run = test_builtins_eq },
  { .skip = 0, .name = "equal?", .run = test_builtins_equal },
  { .skip = 0, .name = "eqv?", .run = test_builtins_eqv },
  { .skip = 0, .name = "symbol", .run = test_builtins_symbol },
  { .skip = 0, .name = "symbol_value", .run = test_builtins_symbol_value },
  { .skip = 0, .name = "vector", .run = test_builtins_vector },
  { .skip = 0, .name = "eval", .run = test_builtins_eval },
  { .skip = 0, .name = "string=?", .run = test_builtins_string_equal },
  { .skip = 0, .name = "concat", .run = test_builtins_concat },
  { .skip = 0, .name = "num->str", .run = test_builtins_number_to_string },
  { .skip = 0, .name = "num->str <=0 ", .run = test_builtins_number_to_string_edge },
  { .skip = 0, .name = "string->number", .run = test_builtins_string_to_number },
  { .skip = 0, .name = "format", .run = test_builtins_format },
  { .skip = 0, .name = "+", .run = test_builtins_sum },
  { .skip = 0, .name = "-", .run = test_builtins_subtract },
  { .skip = 0, .name = "*", .run = test_builtins_multiply },
  { .skip = 0, .name = "/", .run = test_builtins_divide },
  { .skip = 0, .name = "> >= < <=", .run = test_builtins_compare },
  { .skip = 0, .name = "=", .run = test_builtins_num_eq },
  { .skip = 0, .name = "min max", .run = test_builtins_min_max },
  { .skip = 0, .name = "abs", .run = test_builtins_abs },
  { .skip = 0, .name = "and", .run = test_builtins_and },
  { .skip = 0, .name = "or", .run = test_builtins_or },
  { .skip = 0, .name = "if", .run = test_builtins_if },
  { .skip = 0, .name = "when unless", .run = test_builtins_when_unless },
  { .skip = 0, .name = "cond", .run = test_builtins_cond },
  { .skip = 0, .name = "let let*", .run = test_builtins_let },
  { .skip = 0, .name = "def recursion", .run = test_builtins_define_recursion },
  { .skip = 0, .name = "def no leak", .run = test_builtins_define_noleak },
  { .skip = 0, .name = "def in if", .run = test_builtins_define_in_if },
  { .skip = 0, .name = "def shadow", .run = test_builtins_define_shadow_arg },
  { .skip = 0, .name = "def per call", .run = test_builtins_define_per_call },
  { .skip = 0, .name = "def late", .run = test_builtins_define_late },
  { .skip = 0, .name = "def child snap", .run = test_builtins_define_child_snapshot },
  { .skip = 0, .name = "def global", .run = test_builtins_define_global },
  { .skip = 0, .name = "def short", .run = test_builtins_define_short },
  { .skip = 0, .name = "def short error", .run = test_builtins_define_short_error },
  { .skip = 0, .name = "load global", .run = test_builtins_load_global },
  { .skip = 0, .name = "load error", .run = test_builtins_load_error },
  { .skip = 0, .name = "read", .run = test_builtins_read },
  { .skip = 0, .name = "port?", .run = test_builtins_port_p },
  { .skip = 0, .name = "eof-object?", .run = test_builtins_eof_object },
  { .skip = 0, .name = "input str port", .run = test_builtins_input_string_port },
  { .skip = 0, .name = "peek-char", .run = test_builtins_peek_char },
  { .skip = 0, .name = "output str port", .run = test_builtins_output_string_port },
  { .skip = 0, .name = "output str grow", .run = test_builtins_output_string_grow },
  { .skip = 0, .name = "port direction", .run = test_builtins_port_direction },
  { .skip = 0, .name = "close-port", .run = test_builtins_close_port },
  { .skip = 0, .name = "port type err", .run = test_builtins_port_type_errors },
  { .skip = 0, .name = "file port", .run = test_builtins_file_port },
  { .skip = 0, .name = "current port", .run = test_builtins_current_port },
  { .skip = 0, .name = "port gc", .run = test_builtins_port_gc },
  { .skip = 0, .name = "unwind-protect", .run = test_builtins_unwind_protect },
  { .skip = 0, .name = "unw-prot error", .run = test_builtins_unwind_protect_error },
  { .skip = 0, .name = "unw-prot handler", .run = test_builtins_unwind_protect_handler },
  { .skip = 0, .name = "unw-prot cleanup err", .run = test_builtins_unwind_protect_cleanup_error },
  { .skip = 0, .name = "unw-prot gc", .run = test_builtins_unwind_protect_gc },
  { .skip = 0, .name = "cond-case", .run = test_builtins_condition_case },
  { .skip = 0, .name = "cond-case rethrow", .run = test_builtins_condition_case_rethrow },
  { .skip = 0, .name = "cond-case unwind", .run = test_builtins_condition_case_unwind },
  { .skip = 0, .name = "cond-case handlers", .run = test_builtins_condition_case_handlers },
  { .skip = 0, .name = "cond-case gc", .run = test_builtins_condition_case_gc },
  { .skip = 0, .name = "append", .run = test_builtins_append },
  { .skip = 0, .name = "defmacro", .run = test_builtins_defmacro },
  { .skip = 0, .name = "defmacro error", .run = test_builtins_defmacro_error },
  { .skip = 0, .name = "quasiquote", .run = test_builtins_quasiquote },
  { .skip = 0, .name = "quasiquote macro", .run = test_builtins_quasiquote_macro },
  { .skip = 0, .name = "lambda rest", .run = test_builtins_lambda_rest },
  { .skip = 0, .name = "def rest", .run = test_builtins_define_rest },
  { .skip = 0, .name = "defmacro rest", .run = test_builtins_defmacro_rest },
  { .skip = 0, .name = "lambda rest error", .run = test_builtins_lambda_rest_error },
  {}, // terminator
};

TestSuite *
test_suite_builtins ()
{
  return test_suite_init ("builtins", test_builtins_cases);
}

// helpers implementation

static Lisp_Object
parse_src (const char *src)
{
  Lexer l = lex_init (unbox_port (make_input_string_port (make_string (src))));
  Lisp_Object form;
  if (!parse_next_sexp (&l, &form))
    form = q_nil;
  return form;
}

static int
eval_src (const char *src, Lisp_Object *out)
{
  // like load and the repl: macroexpand, then eval
  return condition_case_2 (eval_toplevel, l_globalenv, parse_src (src), out);
}

static int
same_form (Lisp_Object x, Lisp_Object y)
{
  if (eq (x, y))
    return 1;

  if (type_of (x) != type_of (y))
    return 0;

  switch (type_of (x))
    {
    case LISP_STRG:
      return eq (f_string_equal_p (x, y), q_t);
    case LISP_CONS:
      return same_form (f_car (x), f_car (y))
             && same_form (f_cdr (x), f_cdr (y));
    default:
      return 0;
    }
}

static const char *
symname (Lisp_Object symbol)
{
  if (type_of (symbol) != LISP_SYMB)
    return "<not a symbol>";
  return unbox_string (unbox_symbol (symbol)->name)->data;
}

static Lisp_Object
test_catch (Lisp_Object form)
{
  Lisp_Object res;
  if (safe_eval (env_current (), f_car (form), &res))
    return res;
  return f_intern (make_string ("caught"));
}

static void
defsubr_test_catch ()
{
  obarray_put (v_obarray, DEFSUBR ("test-catch", 1, UNEVALLED, test_catch));
}

// test cases implementation

static TestResult
test_builtins_car_cdr ()
{
  EXPECT_INT ("(car '(1 2))", 1);
  EXPECT_FORM ("(cdr '(1 2))", "(2)");
  EXPECT_FORM ("(car nil)", "nil");
  EXPECT_FORM ("(cdr nil)", "nil");
  EXPECT_ERROR ("(car 1)", q_error_type);
  EXPECT_ERROR ("(cdr \"a\")", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_cadr ()
{
  EXPECT_INT ("(cadr '(1 2 3))", 2);
  EXPECT_FORM ("(cadr '(1))", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_cddr ()
{
  EXPECT_FORM ("(cddr '(1 2 3))", "(3)");
  EXPECT_FORM ("(cddr '(1 2))", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_setcar_setcdr ()
{
  // both return the modified cons
  EXPECT_FORM ("(setcar (cons 1 2) 3)", "(3 . 2)");
  EXPECT_FORM ("(setcdr (cons 1 2) 3)", "(1 . 3)");
  EXPECT_ERROR ("(setcar 1 2)", q_error_type);
  EXPECT_ERROR ("(setcdr nil 2)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_length ()
{
  EXPECT_INT ("(length '(1 2 3))", 3);
  EXPECT_INT ("(length nil)", 0);
  EXPECT_INT ("(length \"abcd\")", 4);
  EXPECT_ERROR ("(length 1)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_assoc ()
{
  EXPECT_FORM ("(assoc \"b\" '((\"a\" . 1) (\"b\" . 2)))", "(\"b\" . 2)");
  EXPECT_FORM ("(assoc 2 '((1 . a) (2 . b)))", "(2 . b)");
  EXPECT_FORM ("(assoc \"c\" '((\"a\" . 1) (\"b\" . 2)))", "nil");
  EXPECT_FORM ("(assoc 1 nil)", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_assq ()
{
  EXPECT_FORM ("(assq 'b '((a . 1) (b . 2)))", "(b . 2)");
  EXPECT_FORM ("(assq 'c '((a . 1) (b . 2)))", "nil");
  // strings are different objects: not eq
  EXPECT_FORM ("(assq \"a\" '((\"a\" . 1)))", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_rassoc ()
{
  EXPECT_FORM ("(rassoc \"y\" '((1 . \"x\") (2 . \"y\")))", "(2 . \"y\")");
  EXPECT_FORM ("(rassoc \"z\" '((1 . \"x\") (2 . \"y\")))", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_rassq ()
{
  EXPECT_FORM ("(rassq 'y '((1 . x) (2 . y)))", "(2 . y)");
  EXPECT_FORM ("(rassq 'z '((1 . x) (2 . y)))", "nil");
  EXPECT_FORM ("(rassq \"x\" '((1 . \"x\")))", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_eq ()
{
  EXPECT_FORM ("(eq? 'a 'a)", "t");
  EXPECT_FORM ("(eq? 'a 'b)", "nil");
  EXPECT_FORM ("(eq? 1 1)", "t");
  EXPECT_FORM ("(eq? '(1) '(1))", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_equal ()
{
  EXPECT_FORM ("(equal? 1 1)", "t");
  EXPECT_FORM ("(equal? 1 2)", "nil");
  EXPECT_FORM ("(equal? \"ab\" \"ab\")", "t");
  EXPECT_FORM ("(equal? \"ab\" \"ac\")", "nil");
  EXPECT_FORM ("(equal? 1 \"1\")", "nil");
  EXPECT_FORM ("(equal? '(1 (2 \"x\")) '(1 (2 \"x\")))", "t");
  EXPECT_FORM ("(equal? '(1 2) '(1 3))", "nil");
  EXPECT_FORM ("(equal? '(1 2) '(1))", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_eqv ()
{
  EXPECT_FORM ("(eqv? 'a 'a)", "t");
  EXPECT_FORM ("(eqv? 'a 'b)", "nil");
  EXPECT_FORM ("(eqv? nil nil)", "t");
  // numbers are compared by value, within the same type
  EXPECT_FORM ("(eqv? 1 1)", "t");
  EXPECT_FORM ("(eqv? 1 2)", "nil");
  EXPECT_FORM ("(eqv? (+ 1 2) 3)", "t");
  EXPECT_FORM ("(eqv? 1.5 1.5)", "t");
  EXPECT_FORM ("(eqv? 1.5 2.5)", "nil");
  EXPECT_FORM ("(eqv? (+ 1.0 0.5) 1.5)", "t");
  // an int and a float are never eqv?, even with the same value
  EXPECT_FORM ("(eqv? 1 1.0)", "nil");
  EXPECT_FORM ("(eqv? 1.0 1)", "nil");
  EXPECT_FORM ("(eqv? 0 0.0)", "nil");
  // unlike =, 0.0 and -0.0 are not eqv?
  EXPECT_FORM ("(eqv? 0.0 (- 0.0))", "nil");
  // everything else is compared by identity
  EXPECT_FORM ("(eqv? '(1) '(1))", "nil");
  EXPECT_FORM ("(let ((eqv-x '(1))) (eqv? eqv-x eqv-x))", "t");
  EXPECT_FORM ("(eqv? \"ab\" \"ab\")", "nil");
  EXPECT_FORM ("(let ((eqv-s \"ab\")) (eqv? eqv-s eqv-s))", "t");
  // any type is accepted, no type error
  EXPECT_FORM ("(eqv? 1 \"1\")", "nil");
  EXPECT_FORM ("(eqv? 'a 1.5)", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_symbol ()
{
  Lisp_Object res;
  EXPECT_NO_ERROR ("(symbol \"foo\")", res);
  TEST_CHECK_TYPE ("symbol", res, LISP_SYMB);
  TEST_ASSERT (eq (f_string_equal_p (unbox_symbol (res)->name,
                                     make_string ("foo")),
                   q_t),
               "expected symbol named foo, got %s", symname (res));
  EXPECT_ERROR ("(symbol 1)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_symbol_value ()
{
  Lisp_Object symb = f_intern (make_string ("symbol-value-test"));
  unbox_symbol (symb)->value = box_int (5);
  EXPECT_INT ("(symbol_value 'symbol-value-test)", 5);
  EXPECT_ERROR ("(symbol_value 1)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_vector ()
{
  Lisp_Object res;
  EXPECT_NO_ERROR ("(vector 3)", res);
  TEST_CHECK_TYPE ("vector", res, LISP_VECT);
  EXPECT_ERROR ("(vector \"a\")", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_eval ()
{
  EXPECT_INT ("(eval '(+ 1 2))", 3);
  EXPECT_INT ("(eval 4)", 4);
  EXPECT_FORM ("(eval ''(1 2))", "(1 2)");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_string_equal ()
{
  EXPECT_FORM ("(string=? \"abc\" \"abc\")", "t");
  EXPECT_FORM ("(string=? \"abc\" \"abd\")", "nil");
  EXPECT_FORM ("(string=? \"abc\" \"ab\")", "nil");
  EXPECT_FORM ("(string=? \"\" \"\")", "t");
  EXPECT_ERROR ("(string=? \"a\" 1)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_concat ()
{
  EXPECT_FORM ("(concat \"ab\" \"cd\" \"e\")", "\"abcde\"");
  EXPECT_FORM ("(concat \"ab\")", "\"ab\"");
  EXPECT_FORM ("(concat)", "\"\"");
  EXPECT_INT ("(string-length (concat \"ab\" \"cd\"))", 4);
  EXPECT_ERROR ("(concat \"a\" 1)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_number_to_string ()
{
  EXPECT_FORM ("(number->string 42)", "\"42\"");
  EXPECT_FORM ("(number->string 7)", "\"7\"");
  EXPECT_FORM ("(number->string 1000)", "\"1000\"");
  EXPECT_ERROR ("(number->string \"1\")", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_number_to_string_edge ()
{
  EXPECT_FORM ("(number->string 0)", "\"0\"");
  EXPECT_FORM ("(number->string (- 0 17))", "\"-17\"");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_string_to_number ()
{
  EXPECT_INT ("(string->number \"42\")", 42);
  EXPECT_INT ("(string->number \"-7\")", -7);
  EXPECT_ERROR ("(string->number \"abc\")", q_error_arith);
  EXPECT_ERROR ("(string->number \"12abc\")", q_error_arith);
  EXPECT_ERROR ("(string->number \"99999999999999999999\")", q_error_arith);
  EXPECT_ERROR ("(string->number 1)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_format ()
{
  EXPECT_FORM ("(format nil \"hello\")", "\"hello\"");
  EXPECT_ERROR ("(format 1 \"hello\")", q_error_unimplemented);
  EXPECT_ERROR ("(format nil 1)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_sum ()
{
  EXPECT_INT ("(+)", 0);
  EXPECT_INT ("(+ 5)", 5);
  EXPECT_INT ("(+ 1 2 3)", 6);
  EXPECT_ERROR ("(+ 1 \"a\")", q_error_type);
  // mixed: a single float makes the result a float
  EXPECT_FLOAT ("(+ 1 2 1.5)", 4.5);
  EXPECT_FLOAT ("(+ 1.5 2)", 3.5);
  EXPECT_FLOAT ("(+ 0.25 1 0.25 2)", 3.5);
  // floats summing to a whole number stay floats
  EXPECT_FLOAT ("(+ 0.5 0.5)", 1.0);
  EXPECT_FLOAT ("(+ 1.0)", 1.0);
  EXPECT_ERROR ("(+ 1.5 \"a\")", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_subtract ()
{
  EXPECT_INT ("(- 10 3 2)", 5);
  EXPECT_INT ("(- 3 10)", -7);
  // with a single argument it negates, as in scheme and emacs lisp
  EXPECT_INT ("(- 5)", -5);
  EXPECT_ERROR ("(- \"a\" 1)", q_error_type);
  // mixed: a single float makes the result a float
  EXPECT_FLOAT ("(- 10 2 1.5)", 6.5);
  EXPECT_FLOAT ("(- 1.5 2)", -0.5);
  EXPECT_FLOAT ("(- 0.25 1 0.25 2)", -3.0);
  // floats summing to a whole number stay floats
  EXPECT_FLOAT ("(- 0.5 0.5)", 0.f);
  EXPECT_FLOAT ("(- 1.0)", -1.0);
  EXPECT_ERROR ("(- 1.5 \"a\")", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_multiply ()
{
  EXPECT_INT ("(*)", 1);
  EXPECT_INT ("(* 2 3 4)", 24);
  EXPECT_INT ("(* 2 0)", 0);
  EXPECT_ERROR ("(* 2 \"a\")", q_error_type);
  // mixed: a single float makes the result a float
  EXPECT_FLOAT ("(* 2 3 1.5)", 9.0);
  EXPECT_FLOAT ("(* 1.5 2)", 3.0);
  EXPECT_FLOAT ("(* 0.5 4 0.25 2)", 1.0);
  EXPECT_FLOAT ("(* 0.5 0.5)", 0.25);
  // floats multiplying to a whole number stay floats
  EXPECT_FLOAT ("(* 0.5 2)", 1.0);
  EXPECT_FLOAT ("(* 0 1.5)", 0.0);
  EXPECT_FLOAT ("(* 2.0)", 2.0);
  EXPECT_ERROR ("(* 1.5 \"a\")", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_divide ()
{
  EXPECT_INT ("(/ 20 2 5)", 2);
  EXPECT_INT ("(/ 7 2)", 3);
  EXPECT_ERROR ("(/ 1 0)", q_error_arith);
  EXPECT_ERROR ("(/ 1 \"a\")", q_error_type);
  // mixed: a single float makes the result a float
  EXPECT_FLOAT ("(/ 6 3 0.5)", 4.0);
  EXPECT_FLOAT ("(/ 1.5 2)", 0.75);
  EXPECT_FLOAT ("(/ 0.5 4 0.25 2)", 0.25);
  EXPECT_FLOAT ("(/ 0.5 0.5)", 1.f);
  // floats multiplying to a whole number stay floats
  EXPECT_FLOAT ("(/ 0.5 2)", 0.25);
  EXPECT_FLOAT ("(/ 0 1.5)", 0.0);
  EXPECT_FLOAT ("(/ 2.0)", 0.5);
  EXPECT_ERROR ("(/ 1.0 0.0)", q_error_arith);
  EXPECT_ERROR ("(/ 1 0.0)", q_error_arith);
  EXPECT_ERROR ("(/ 1.5 \"a\")", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_compare ()
{
  EXPECT_FORM ("(> 2 1)", "t");
  EXPECT_FORM ("(> 1 1)", "nil");
  EXPECT_FORM ("(>= 1 1)", "t");
  EXPECT_FORM ("(>= 0 1)", "nil");
  EXPECT_FORM ("(< 1 2)", "t");
  EXPECT_FORM ("(< 2 2)", "nil");
  EXPECT_FORM ("(<= 2 2)", "t");
  EXPECT_FORM ("(<= 3 2)", "nil");
  EXPECT_ERROR ("(< 1 \"a\")", q_error_type);
  // floats
  EXPECT_FORM ("(> 2.5 1.5)", "t");
  EXPECT_FORM ("(> 1.5 1.5)", "nil");
  EXPECT_FORM ("(>= 1.5 1.5)", "t");
  EXPECT_FORM ("(>= 0.5 1.5)", "nil");
  EXPECT_FORM ("(< 0.25 0.5)", "t");
  EXPECT_FORM ("(< 0.5 0.5)", "nil");
  EXPECT_FORM ("(<= 0.5 0.5)", "t");
  EXPECT_FORM ("(<= 0.75 0.5)", "nil");
  // mixed int and float, in both argument orders
  EXPECT_FORM ("(> 2 1.5)", "t");
  EXPECT_FORM ("(> 1.5 2)", "nil");
  EXPECT_FORM ("(>= 2.5 2)", "t");
  EXPECT_FORM ("(>= 1 1.5)", "nil");
  EXPECT_FORM ("(< 1 1.5)", "t");
  EXPECT_FORM ("(< 2.5 2)", "nil");
  EXPECT_FORM ("(<= 1.5 2)", "t");
  EXPECT_FORM ("(<= 2 1.5)", "nil");
  // an int and a float with the same value are equal
  EXPECT_FORM ("(> 1 1.0)", "nil");
  EXPECT_FORM ("(> 1.0 1)", "nil");
  EXPECT_FORM ("(>= 1 1.0)", "t");
  EXPECT_FORM ("(>= 1.0 1)", "t");
  EXPECT_FORM ("(< 1 1.0)", "nil");
  EXPECT_FORM ("(< 1.0 1)", "nil");
  EXPECT_FORM ("(<= 1 1.0)", "t");
  EXPECT_FORM ("(<= 1.0 1)", "t");
  // negatives (there are no negative literals, so they are built with -)
  EXPECT_FORM ("(< (- 1.5) 1)", "t");
  EXPECT_FORM ("(> (- 1) (- 1.5))", "t");
  EXPECT_FORM ("(<= (- 0.5) 0)", "t");
  EXPECT_FORM ("(>= (- 2) (- 1.5))", "nil");
  // type errors with floats, on either argument
  EXPECT_ERROR ("(> 1.5 \"a\")", q_error_type);
  EXPECT_ERROR ("(>= \"a\" 1.5)", q_error_type);
  EXPECT_ERROR ("(< 1.5 \"a\")", q_error_type);
  EXPECT_ERROR ("(<= \"a\" 1)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_num_eq ()
{
  EXPECT_FORM ("(= 1 1)", "t");
  EXPECT_FORM ("(= 1 2)", "nil");
  EXPECT_FORM ("(= (+ 1 2) 3)", "t");
  EXPECT_FORM ("(= 1.5 1.5)", "t");
  EXPECT_FORM ("(= 1.5 2.5)", "nil");
  // ints and floats are compared by numeric value, in both argument orders
  EXPECT_FORM ("(= 1 1.0)", "t");
  EXPECT_FORM ("(= 1.0 1)", "t");
  EXPECT_FORM ("(= 0 0.0)", "t");
  EXPECT_FORM ("(= 2 1.5)", "nil");
  EXPECT_FORM ("(= 1.5 2)", "nil");
  EXPECT_FORM ("(= (- 1) (- 1.0))", "t");
  // IEEE 754: 0.0 and -0.0 are equal
  EXPECT_FORM ("(= 0.0 (- 0.0))", "t");
  EXPECT_FORM ("(= 0 (- 0.0))", "t");
  // only numbers are accepted, on either argument
  EXPECT_ERROR ("(= 'a 'a)", q_error_type);
  EXPECT_ERROR ("(= 1 \"a\")", q_error_type);
  EXPECT_ERROR ("(= \"a\" 1.5)", q_error_type);
  EXPECT_ERROR ("(= 1.5 'a)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_min_max ()
{
  EXPECT_INT ("(max 5)", 5);
  EXPECT_INT ("(min 5)", 5);
  EXPECT_INT ("(max 1 3 2)", 3);
  EXPECT_INT ("(min 2 1 3)", 1);
  EXPECT_INT ("(max 2 2)", 2);
  EXPECT_INT ("(min 2 2)", 2);
  EXPECT_INT ("(max (- 3) (- 1) (- 2))", -1);
  EXPECT_INT ("(min (- 3) (- 1) (- 2))", -3);
  EXPECT_FLOAT ("(max 1.5)", 1.5);
  EXPECT_FLOAT ("(min 1.5)", 1.5);
  EXPECT_FLOAT ("(max 0.5 1.5 0.25)", 1.5);
  EXPECT_FLOAT ("(min 0.5 1.5 0.25)", 0.25);
  // mixed: a single float makes the result a float, even when the winner
  // is an int
  EXPECT_FLOAT ("(max 1 2.5)", 2.5);
  EXPECT_FLOAT ("(max 2.5 1)", 2.5);
  EXPECT_FLOAT ("(max 3 2.5)", 3.0);
  EXPECT_FLOAT ("(max 2.5 3)", 3.0);
  EXPECT_FLOAT ("(min 1 2.5)", 1.0);
  EXPECT_FLOAT ("(min 2.5 1)", 1.0);
  EXPECT_FLOAT ("(min 3 2.5)", 2.5);
  EXPECT_FLOAT ("(max 1 2 0.5 3)", 3.0);
  EXPECT_FLOAT ("(min 1 2 0.5 3)", 0.5);
  // only numbers are accepted, at any position
  EXPECT_ERROR ("(max \"a\")", q_error_type);
  EXPECT_ERROR ("(min 'a 1)", q_error_type);
  EXPECT_ERROR ("(max 1 \"a\")", q_error_type);
  EXPECT_ERROR ("(min 1.5 'a)", q_error_type);
  EXPECT_ERROR ("(max 1 2.5 \"a\")", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_abs ()
{
  EXPECT_INT ("(abs 5)", 5);
  EXPECT_INT ("(abs (- 5))", 5);
  EXPECT_INT ("(abs 0)", 0);
  EXPECT_FLOAT ("(abs 1.5)", 1.5);
  EXPECT_FLOAT ("(abs (- 1.5))", 1.5);
  EXPECT_FLOAT ("(abs 0.0)", 0.0);
  // EXPECT_FLOAT uses == which can't tell 0.0 from -0.0: eqv? compares bits
  EXPECT_FORM ("(eqv? (abs 0.0) 0.0)", "t");
  EXPECT_FORM ("(eqv? (abs (- 0.0)) 0.0)", "t");
  EXPECT_ERROR ("(abs \"a\")", q_error_type);
  EXPECT_ERROR ("(abs 'a)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_and ()
{
  EXPECT_INT ("(and 1 2 3)", 3);
  EXPECT_FORM ("(and 1 nil 3)", "nil");
  // stops at the first nil
  EXPECT_FORM ("(and nil (car 1))", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_or ()
{
  EXPECT_INT ("(or nil 2 3)", 2);
  EXPECT_FORM ("(or nil nil)", "nil");
  // stops at the first non nil
  EXPECT_INT ("(or 1 (car 1))", 1);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_if ()
{
  EXPECT_INT ("(if t 1 2)", 1);
  EXPECT_INT ("(if nil 1 2)", 2);
  EXPECT_INT ("(if 0 1 2)", 1);
  EXPECT_FORM ("(if nil 1)", "nil");
  // else branch is an implicit progn
  EXPECT_INT ("(if nil 1 2 3)", 3);
  // the untaken branch is not evaluated
  EXPECT_INT ("(if t 1 (car 1))", 1);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_when_unless ()
{
  EXPECT_INT ("(when t 1 2)", 2);
  EXPECT_FORM ("(when nil 1)", "nil");
  EXPECT_FORM ("(when nil (car 1))", "nil");
  EXPECT_INT ("(unless nil 1 2)", 2);
  EXPECT_FORM ("(unless t 1)", "nil");
  EXPECT_FORM ("(unless t (car 1))", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_cond ()
{
  EXPECT_INT ("(cond (nil 1) (t 2))", 2);
  EXPECT_INT ("(cond ((< 1 2) 1) (t 2))", 1);
  EXPECT_FORM ("(cond (nil 1))", "nil");
  EXPECT_INT ("(cond (nil (car 1)) (t 2))", 2);
  // clause body is an implicit progn, as in scheme and emacs lisp
  EXPECT_INT ("(cond (t 1 2))", 2);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_let ()
{
  EXPECT_INT ("(let ((let-a 1) (let-b 2)) (+ let-a let-b))", 3);
  EXPECT_INT ("(let* ((let-c 1) (let-d (+ let-c 1))) let-d)", 2);
  EXPECT_INT ("(let ((let-e 1)) (let ((let-e 2)) let-e))", 2);
  // body is an implicit progn
  EXPECT_INT ("(let ((let-f 1)) let-f 5)", 5);
  EXPECT_INT ("(let () 7)", 7);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_define_recursion ()
{
  // the inner lambda captures the env of the call, so it sees its own name
  EXPECT_NO_ERROR_SRC (
      "(define def-rec (lambda (n)"
      "  (define def-rec-loop (lambda (i acc)"
      "    (if (eq? i 0) acc (def-rec-loop (- i 1) (+ acc 1)))))"
      "  (def-rec-loop n 0)))");
  EXPECT_INT ("(def-rec 5)", 5);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_define_noleak ()
{
  // the env of a call with no args is empty, but it is still local
  EXPECT_NO_ERROR_SRC (
      "(define def-noleak (lambda () (define def-noleak-x 1) def-noleak-x))");
  EXPECT_INT ("(def-noleak)", 1);
  EXPECT_ERROR ("def-noleak-x", q_error_unbound);

  // same for a let with no bindings at top level
  EXPECT_INT ("(let () (define def-let-x 1) def-let-x)", 1);
  EXPECT_ERROR ("def-let-x", q_error_unbound);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_define_in_if ()
{
  // define changes the env of the body, not the frame of if
  EXPECT_NO_ERROR_SRC (
      "(define def-if (lambda (c) (if c (define def-if-x 1)) def-if-x))");
  EXPECT_INT ("(def-if t)", 1);
  EXPECT_ERROR ("(def-if nil)", q_error_unbound);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_define_shadow_arg ()
{
  // the define goes in front of the arg in the alist
  EXPECT_NO_ERROR_SRC (
      "(define def-shadow (lambda (def-shadow-x)"
      "  (define def-shadow-x 2) def-shadow-x))");
  EXPECT_INT ("(def-shadow 1)", 2);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_define_per_call ()
{
  // every call has its own env: closures do not share the inner define
  EXPECT_NO_ERROR_SRC (
      "(define def-mk (lambda (v) (define def-mk-x v) (lambda () def-mk-x)))");
  EXPECT_NO_ERROR_SRC ("(define def-mk-c1 (def-mk 1))");
  EXPECT_NO_ERROR_SRC ("(define def-mk-c2 (def-mk 2))");
  EXPECT_INT ("(def-mk-c1)", 1);
  EXPECT_INT ("(def-mk-c2)", 2);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_define_late ()
{
  // a closure sees the defines done later in the env it captured
  EXPECT_NO_ERROR_SRC (
      "(define def-late (lambda ()"
      "  (define def-late-g (lambda () (def-late-h)))"
      "  (define def-late-h (lambda () 7))"
      "  (def-late-g)))");
  EXPECT_INT ("(def-late)", 7);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_define_child_snapshot ()
{
  // a child env copies the bindings of the parent when it is created: the
  // closure returned by def-snap-mk does not see def-snap-l, defined later
  EXPECT_NO_ERROR_SRC (
      "(define def-snap (lambda ()"
      "  (define def-snap-mk (lambda () (lambda () (def-snap-l))))"
      "  (define def-snap-c (def-snap-mk))"
      "  (define def-snap-l (lambda () 1))"
      "  (def-snap-c)))");
  EXPECT_ERROR ("(def-snap)", q_error_unbound);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_define_global ()
{
  // a local define shadows the global without changing it
  EXPECT_NO_ERROR_SRC ("(define def-glob 3)");
  EXPECT_INT ("(let ((def-glob-y 1)) def-glob)", 3);
  EXPECT_INT ("(let ((def-glob-y 1)) (define def-glob 4) def-glob)", 4);
  EXPECT_INT ("def-glob", 3);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_define_short ()
{
  // (define (f args) body) is the same as (define f (lambda (args) body))
  EXPECT_NO_ERROR_SRC ("(define (def-sh-add a b) (+ a b))");
  EXPECT_INT ("(def-sh-add 2 3)", 5);

  // no args, body with more forms
  EXPECT_NO_ERROR_SRC ("(define (def-sh-seq) 1 2 3)");
  EXPECT_INT ("(def-sh-seq)", 3);

  // recursion through the global name
  EXPECT_NO_ERROR_SRC (
      "(define (def-sh-count n) (if (eq? n 0) 0 (+ 1 (def-sh-count (- n 1)))))");
  EXPECT_INT ("(def-sh-count 4)", 4);

  // inner short define: local, and it captures the args of the call
  EXPECT_NO_ERROR_SRC (
      "(define (def-sh-outer x)"
      "  (define (def-sh-inner y) (+ x y))"
      "  (def-sh-inner 10))");
  EXPECT_INT ("(def-sh-outer 1)", 11);
  EXPECT_ERROR ("def-sh-inner", q_error_unbound);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_define_short_error ()
{
  // the name must be a symbol, in both forms
  EXPECT_ERROR ("(define (1 x) x)", q_error_type);
  EXPECT_ERROR ("(define ((def-sh-e a) b) b)", q_error_type);
  EXPECT_ERROR ("(define 1 2)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_load_global ()
{
  // load evaluates in global scope even when called from a lambda
  EXPECT_NO_ERROR_SRC (
      "(define load-global (lambda ()"
      "  (load \"test/assets/src-load-global.tl\")))");
  EXPECT_NO_ERROR_SRC ("(load-global)");
  EXPECT_INT ("load-global-a", 1);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_load_error ()
{
  // the definitions before the error survive the unwinding of load
  EXPECT_ERROR ("(load \"test/assets/src-load-error.tl\")",
                f_intern (make_string ("load-test-error")));
  EXPECT_INT ("load-error-c", 1);
  EXPECT_ERROR ("load-error-d", q_error_unbound);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_read ()
{
  EXPECT_INT ("(read (open-input-string \"42\"))", 42);
  EXPECT_FORM ("(read (open-input-string \"(a (b . c) 1)\"))",
               "(a (b . c) 1)");
  EXPECT_INT ("(eval (read (open-input-string \"(+ 1 2)\")))", 3);
  // syntax errors are signaled, not fatal
  EXPECT_ERROR ("(read (open-input-string \"(1 2\"))", q_error_syntax);
  EXPECT_ERROR ("(read (open-input-string \")\"))", q_error_syntax);
  EXPECT_ERROR ("(read 1)", q_error_type);
  EXPECT_ERROR ("(read \"42\")", q_error_type);
  EXPECT_ERROR ("(read (open-output-string))", q_error_portinvalid);
  // consecutive reads continue from where the previous one stopped
  EXPECT_FORM ("(let ((p (open-input-string \"(a)b 42\")))"
               "  (read p)"
               "  (read p))",
               "b");
  EXPECT_INT ("(let ((p (open-input-string \"(a)b 42\")))"
              "  (read p)"
              "  (read p)"
              "  (read p))",
              42);
  // the char ending a form stays in the port
  EXPECT_FORM ("(let ((p (open-input-string \"foo)\")))"
               "  (read p)"
               "  (read-char p))",
               "?)");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_port_p ()
{
  EXPECT_FORM ("(input-port? (open-input-string \"a\"))", "t");
  EXPECT_FORM ("(output-port? (open-input-string \"a\"))", "nil");
  EXPECT_FORM ("(input-port? (open-output-string))", "nil");
  EXPECT_FORM ("(output-port? (open-output-string))", "t");
  // any object is accepted, no type error
  EXPECT_FORM ("(input-port? 1)", "nil");
  EXPECT_FORM ("(output-port? \"a\")", "nil");
  EXPECT_FORM ("(input-port? nil)", "nil");
  // ports evaluate to themselves
  EXPECT_FORM ("(input-port? (eval (open-input-string \"a\")))", "t");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_eof_object ()
{
  EXPECT_FORM ("(eof-object? (read-char (open-input-string \"\")))", "t");
  EXPECT_FORM ("(eof-object? (read-char (open-input-string \"a\")))", "nil");
  EXPECT_FORM ("(eof-object? 1)", "nil");
  EXPECT_FORM ("(eof-object? nil)", "nil");
  // the eof object can't be built by naming it
  EXPECT_FORM ("(eof-object? 'eof)", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_input_string_port ()
{
  EXPECT_NO_ERROR_SRC ("(define port-in (open-input-string \"ab\"))");
  EXPECT_FORM ("(read-char port-in)", "?a");
  EXPECT_FORM ("(read-char port-in)", "?b");
  EXPECT_FORM ("(eof-object? (read-char port-in))", "t");
  // eof is sticky
  EXPECT_FORM ("(eof-object? (read-char port-in))", "t");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_peek_char ()
{
  EXPECT_NO_ERROR_SRC ("(define port-peek (open-input-string \"ab\"))");
  // peek doesn't consume
  EXPECT_FORM ("(peek-char port-peek)", "?a");
  EXPECT_FORM ("(peek-char port-peek)", "?a");
  EXPECT_FORM ("(read-char port-peek)", "?a");
  EXPECT_FORM ("(read-char port-peek)", "?b");
  // peek at the end gives eof, and so does the following read
  EXPECT_FORM ("(eof-object? (peek-char port-peek))", "t");
  EXPECT_FORM ("(eof-object? (read-char port-peek))", "t");
  // a char returned by peek is not lost when read later
  EXPECT_NO_ERROR_SRC ("(define port-peek2 (open-input-string \"xy\"))");
  EXPECT_FORM ("(progn (peek-char port-peek2) (read-char port-peek2))", "?x");
  EXPECT_FORM ("(read-char port-peek2)", "?y");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_output_string_port ()
{
  EXPECT_FORM ("(get-output-string (open-output-string))", "\"\"");
  EXPECT_NO_ERROR_SRC ("(define port-out (open-output-string))");
  EXPECT_NO_ERROR_SRC ("(write-char ?a port-out)");
  EXPECT_NO_ERROR_SRC ("(write-char ?b port-out)");
  EXPECT_FORM ("(get-output-string port-out)", "\"ab\"");
  // get-output-string doesn't reset the port, writes keep accumulating
  EXPECT_NO_ERROR_SRC ("(write-char ?c port-out)");
  EXPECT_FORM ("(get-output-string port-out)", "\"abc\"");
  // the returned string is a copy
  EXPECT_NO_ERROR_SRC ("(define port-out-s (get-output-string port-out))");
  EXPECT_NO_ERROR_SRC ("(write-char ?d port-out)");
  EXPECT_FORM ("port-out-s", "\"abc\"");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_output_string_grow ()
{
  // writes 2^n chars with recursion depth n, to go past the initial buffer
  EXPECT_NO_ERROR_SRC (
      "(define port-fill (lambda (p n)"
      "  (if (eq? n 0) (write-char ?x p)"
      "    (port-fill p (- n 1)) (port-fill p (- n 1)))))");
  EXPECT_NO_ERROR_SRC ("(define port-big (open-output-string))");
  EXPECT_NO_ERROR_SRC ("(port-fill port-big 11)");
  EXPECT_INT ("(string-length (get-output-string port-big))", 2048);
  // what was written before the growth is still there
  EXPECT_NO_ERROR_SRC ("(define port-big-in"
                       "  (open-input-string (get-output-string port-big)))");
  EXPECT_FORM ("(read-char port-big-in)", "?x");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_port_direction ()
{
  EXPECT_ERROR ("(read-char (open-output-string))", q_error_portinvalid);
  EXPECT_ERROR ("(peek-char (open-output-string))", q_error_portinvalid);
  EXPECT_ERROR ("(char-ready? (open-output-string))", q_error_portinvalid);
  EXPECT_ERROR ("(write-char ?a (open-input-string \"\"))",
                q_error_portinvalid);
  EXPECT_ERROR ("(flush-output-port (open-input-string \"\"))",
                q_error_portinvalid);
  EXPECT_ERROR ("(get-output-string (open-input-string \"\"))",
                q_error_portinvalid);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_close_port ()
{
  EXPECT_NO_ERROR_SRC ("(define port-cl-in (open-input-string \"ab\"))");
  EXPECT_NO_ERROR_SRC ("(define port-cl-out (open-output-string))");
  // closing twice is allowed, only the first time does something
  EXPECT_FORM ("(close-port port-cl-in)", "t");
  EXPECT_FORM ("(close-port port-cl-in)", "nil");
  EXPECT_FORM ("(close-port port-cl-out)", "t");
  // a closed port is still a port
  EXPECT_FORM ("(input-port? port-cl-in)", "t");
  EXPECT_ERROR ("(read-char port-cl-in)", q_error_portclosed);
  EXPECT_ERROR ("(peek-char port-cl-in)", q_error_portclosed);
  EXPECT_ERROR ("(char-ready? port-cl-in)", q_error_portclosed);
  EXPECT_ERROR ("(write-char ?a port-cl-out)", q_error_portclosed);
  EXPECT_ERROR ("(flush-output-port port-cl-out)", q_error_portclosed);
  EXPECT_ERROR ("(get-output-string port-cl-out)", q_error_portclosed);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_port_type_errors ()
{
  EXPECT_ERROR ("(open-input-string 1)", q_error_type);
  EXPECT_ERROR ("(open-input-file 1)", q_error_type);
  EXPECT_ERROR ("(open-output-file 'a)", q_error_type);
  EXPECT_ERROR ("(read-char 1)", q_error_type);
  EXPECT_ERROR ("(peek-char \"a\")", q_error_type);
  EXPECT_ERROR ("(char-ready? 1)", q_error_type);
  EXPECT_ERROR ("(write-char ?a 1)", q_error_type);
  EXPECT_ERROR ("(write-char 42 (open-output-string))", q_error_type);
  EXPECT_ERROR ("(write-char \"a\" (open-output-string))", q_error_type);
  EXPECT_ERROR ("(flush-output-port 1)", q_error_type);
  EXPECT_ERROR ("(get-output-string 1)", q_error_type);
  EXPECT_ERROR ("(close-port 1)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_file_port ()
{
  EXPECT_ERROR ("(open-input-file \"test/assets/does-not-exist\")",
                q_error_file);
  // a directory can't be opened for writing
  EXPECT_ERROR ("(open-output-file \"test/assets\")", q_error_file);

  // obj/ is created by the build and ignored by git
  EXPECT_NO_ERROR_SRC (
      "(define port-fout (open-output-file \"obj/test-port.txt\"))");
  EXPECT_FORM ("(output-port? port-fout)", "t");
  EXPECT_NO_ERROR_SRC ("(write-char ?h port-fout)");
  EXPECT_NO_ERROR_SRC ("(write-char ?i port-fout)");
  EXPECT_FORM ("(flush-output-port port-fout)", "nil");
  EXPECT_FORM ("(close-port port-fout)", "t");

  EXPECT_NO_ERROR_SRC (
      "(define port-fin (open-input-file \"obj/test-port.txt\"))");
  EXPECT_FORM ("(input-port? port-fin)", "t");
  EXPECT_FORM ("(peek-char port-fin)", "?h");
  EXPECT_FORM ("(read-char port-fin)", "?h");
  EXPECT_FORM ("(read-char port-fin)", "?i");
  EXPECT_FORM ("(eof-object? (read-char port-fin))", "t");
  EXPECT_FORM ("(close-port port-fin)", "t");

  // opening for writing truncates
  EXPECT_NO_ERROR_SRC ("(close-port (open-output-file \"obj/test-port.txt\"))");
  EXPECT_NO_ERROR_SRC (
      "(define port-fin2 (open-input-file \"obj/test-port.txt\"))");
  EXPECT_FORM ("(eof-object? (read-char port-fin2))", "t");
  EXPECT_NO_ERROR_SRC ("(close-port port-fin2)");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_current_port ()
{
  EXPECT_FORM ("(input-port? (current-input-port))", "t");
  EXPECT_FORM ("(output-port? (current-output-port))", "t");
  EXPECT_FORM ("(eq? (current-output-port) (current-output-port))", "t");
  // without a port argument the current port is used. read-char and
  // write-char are not tested this way: they would block on stdin or
  // print in the middle of the test output
  EXPECT_FORM ("(char-ready?)", "t");
  EXPECT_FORM ("(flush-output-port)", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_port_gc ()
{
  // a port reachable from a variable survives a gc, with its state
  EXPECT_NO_ERROR_SRC ("(define port-gc (open-input-string \"ab\"))");
  EXPECT_FORM ("(read-char port-gc)", "?a");
  EXPECT_NO_ERROR_SRC ("(gc)");
  EXPECT_FORM ("(read-char port-gc)", "?b");

  // the peeked char survives too
  EXPECT_NO_ERROR_SRC ("(define port-gc2 (open-input-string \"xy\"))");
  EXPECT_FORM ("(peek-char port-gc2)", "?x");
  EXPECT_NO_ERROR_SRC ("(gc)");
  EXPECT_FORM ("(read-char port-gc2)", "?x");

  // the source string of an input port can be collected, the port must
  // still read it
  EXPECT_NO_ERROR_SRC (
      "(define port-gc3 (open-input-string (concat \"q\" \"r\")))");
  EXPECT_NO_ERROR_SRC ("(gc)");
  EXPECT_FORM ("(read-char port-gc3)", "?q");
  EXPECT_FORM ("(read-char port-gc3)", "?r");

  // the current ports are gc roots: if they were collected, a new port
  // could take their place
  EXPECT_NO_ERROR_SRC ("(gc)");
  EXPECT_NO_ERROR_SRC ("(open-input-string \"z\")");
  EXPECT_NO_ERROR_SRC ("(open-output-string)");
  EXPECT_FORM ("(input-port? (current-input-port))", "t");
  EXPECT_FORM ("(output-port? (current-output-port))", "t");

  // the eof object is a gc root: if it was collected, a new symbol could
  // take its place
  EXPECT_NO_ERROR_SRC ("(gc)");
  EXPECT_NO_ERROR_SRC ("(symbol \"port-gc-sym\")");
  EXPECT_FORM ("(eof-object? (symbol \"port-gc-sym\"))", "nil");
  EXPECT_FORM ("(eof-object? (read-char (open-input-string \"\")))", "t");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_unwind_protect ()
{
  // local exit: the value is the body one, the cleanups run in order
  EXPECT_NO_ERROR_SRC ("(define up-log nil)");
  EXPECT_INT ("(unwind-protect (+ 40 2)"
              "  (set! up-log (cons 1 up-log))"
              "  (set! up-log (cons 2 up-log)))",
              42);
  EXPECT_FORM ("up-log", "(2 1)");

  // no cleanups
  EXPECT_INT ("(unwind-protect 7)", 7);

  // cleanups are forms, not only calls
  EXPECT_INT ("(unwind-protect 1 2 'x up-log)", 1);

  // cleanups see the locals of the unwind-protect env
  EXPECT_INT ("(let ((x 5)) (unwind-protect (+ x 1) (set! up-log x)))", 6);
  EXPECT_INT ("up-log", 5);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_unwind_protect_error ()
{
  Lisp_Object boom = f_intern (make_string ("up-boom"));

  // the cleanups run from the innermost, then the error propagates
  EXPECT_NO_ERROR_SRC ("(define up-log nil)");
  EXPECT_ERROR ("(unwind-protect"
                "  (unwind-protect (signal 'up-boom)"
                "    (set! up-log (cons 'inner up-log)))"
                "  (set! up-log (cons 'outer up-log)))",
                boom);
  EXPECT_FORM ("up-log", "(outer inner)");

  // through function calls, each cleanup with the locals of its own env
  EXPECT_NO_ERROR_SRC ("(define up-log nil)");
  EXPECT_NO_ERROR_SRC ("(define up-throw (lambda (a) (signal 'up-boom a)))");
  EXPECT_NO_ERROR_SRC ("(define up-f (lambda (x)"
                       "  (unwind-protect (up-throw x)"
                       "    (set! up-log (cons x up-log)))))");
  EXPECT_ERROR ("(let ((y 1))"
                "  (unwind-protect (up-f 2)"
                "    (set! up-log (cons y up-log))))",
                boom);
  EXPECT_FORM ("up-log", "(1 2)");

  // the cleanups do not change the backtrace, innermost frame first
  Lisp_Object err;
  TEST_ASSERT (!eval_src ("(unwind-protect (up-throw 1) 2)", &err),
               "expected up-boom, got no error");
  TEST_ASSERT (
      same_form (f_error_backtrace (err),
                 parse_src ("(\"signal\" \"up-throw\" \"unwind-protect\")")),
      "expected backtrace (\"signal\" \"up-throw\" \"unwind-protect\")");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_unwind_protect_handler ()
{
  defsubr_test_catch ();
  EXPECT_FORM ("(test-catch (signal 'up-boom))", "caught");

  // the handler is between the two unwind-protects: the error runs only the
  // inner cleanup, the outer one runs at its own exit
  EXPECT_NO_ERROR_SRC ("(define up-log nil)");
  EXPECT_NO_ERROR_SRC (
      "(unwind-protect"
      "  (progn"
      "    (test-catch (unwind-protect (signal 'up-boom)"
      "                  (set! up-log (cons 'inner up-log))))"
      "    (set! up-log (cons 'after up-log)))"
      "  (set! up-log (cons 'outer up-log)))");
  EXPECT_FORM ("up-log", "(outer after inner)");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_unwind_protect_cleanup_error ()
{
  Lisp_Object boom2 = f_intern (make_string ("up-boom2"));

  // an error in a cleanup replaces the one in flight: the rest of that
  // cleanup is skipped and it is not run again, the outer cleanups still run
  EXPECT_NO_ERROR_SRC ("(define up-log nil)");
  EXPECT_ERROR ("(unwind-protect"
                "  (unwind-protect (signal 'up-boom)"
                "    (set! up-log (cons 'c1 up-log))"
                "    (signal 'up-boom2)"
                "    (set! up-log (cons 'c1-rest up-log)))"
                "  (set! up-log (cons 'c2 up-log)))",
                boom2);
  EXPECT_FORM ("up-log", "(c2 c1)");

  // same at local exit
  EXPECT_NO_ERROR_SRC ("(define up-log nil)");
  EXPECT_ERROR ("(unwind-protect 1"
                "  (set! up-log (cons 'c up-log))"
                "  (signal 'up-boom2))",
                boom2);
  EXPECT_FORM ("up-log", "(c)");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_unwind_protect_gc ()
{
  // the conses allocated after the gc try to reuse the blocks of what was
  // wrongly freed

  // local exit: the body value survives a gc in the cleanups
  EXPECT_FORM ("(unwind-protect (cons 1 (cons 2 nil))"
               "  (gc)"
               "  (cons (cons 0 0) (cons 0 0)))",
               "(1 2)");

  // non local exit: the error, the remaining cleanups and the env survive a
  // gc in the cleanups
  EXPECT_NO_ERROR_SRC ("(define up-log nil)");
  EXPECT_NO_ERROR_SRC ("(define up-throw (lambda (a) (signal 'up-boom a)))");
  Lisp_Object err;
  TEST_ASSERT (!eval_src ("(let ((l (cons 1 (cons 2 nil))))"
                          "  (unwind-protect (up-throw (cons 3 4))"
                          "    (gc)"
                          "    (cons (cons 0 0) (cons 0 0))"
                          "    (set! up-log (car (cdr l)))))",
                          &err),
               "expected up-boom, got no error");
  TEST_ASSERT (eq (f_error_symbol (err), f_intern (make_string ("up-boom"))),
               "expected up-boom, got %s", symname (f_error_symbol (err)));
  TEST_ASSERT (same_form (f_error_data (err), parse_src ("(3 . 4)")),
               "expected error data (3 . 4)");
  EXPECT_INT ("up-log", 2);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_condition_case ()
{
  // no error: the value is the body one, no handler runs
  EXPECT_INT ("(condition-case (+ 40 2) (cc-boom 0))", 42);

  // caught by symbol: the value is the handler one
  EXPECT_FORM ("(condition-case (signal 'cc-boom) (cc-boom 'caught))",
               "caught");

  // the handler body is a progn
  EXPECT_INT ("(condition-case (signal 'cc-boom) (cc-boom 1 2 3))", 3);

  // the first matching clause wins, non matching ones are skipped
  EXPECT_FORM ("(condition-case (signal 'cc-boom)"
               "  (cc-other 'other)"
               "  (cc-boom 'first)"
               "  (cc-boom 'second))",
               "first");

  // t catches everything
  EXPECT_FORM ("(condition-case (signal 'cc-boom) (cc-other 'other) (t 'all))",
               "all");

  // builtin errors are caught by their symbol
  EXPECT_FORM ("(condition-case (car 1) (type-error 'type))", "type");
  EXPECT_FORM ("(condition-case cc-unbound-var (unbound-error 'unbound))",
               "unbound");

  // the handler sees the locals of the condition-case env
  EXPECT_INT ("(let ((x 5)) (condition-case (signal 'cc-boom) (cc-boom x)))",
              5);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_condition_case_rethrow ()
{
  Lisp_Object boom = f_intern (make_string ("cc-boom"));
  Lisp_Object boom2 = f_intern (make_string ("cc-boom2"));

  // no matching clause: the error propagates unchanged
  EXPECT_ERROR ("(condition-case (signal 'cc-boom) (cc-other 'other))", boom);

  // and is caught by an outer condition-case
  EXPECT_FORM ("(condition-case"
               "  (condition-case (signal 'cc-boom) (cc-other 'inner))"
               "  (cc-boom 'outer))",
               "outer");

  // the rethrown error keeps its data and its original backtrace: a rebuilt
  // one would start from the condition-case frame
  EXPECT_NO_ERROR_SRC ("(define cc-throw (lambda (a) (signal 'cc-boom a)))");
  Lisp_Object err;
  TEST_ASSERT (
      !eval_src ("(condition-case (cc-throw (cons 3 4)) (cc-other 'other))",
                 &err),
      "expected cc-boom, got no error");
  TEST_ASSERT (eq (f_error_symbol (err), boom), "expected cc-boom, got %s",
               symname (f_error_symbol (err)));
  TEST_ASSERT (same_form (f_error_data (err), parse_src ("(3 . 4)")),
               "expected error data (3 . 4)");
  Lisp_Object bt = f_error_backtrace (err);
  TEST_ASSERT (same_form (f_car (bt), parse_src ("\"signal\""))
                   && same_form (f_car (f_cdr (bt)), parse_src ("\"cc-throw\"")),
               "expected backtrace starting with (\"signal\" \"cc-throw\")");

  // an error in a handler is not caught by its own condition-case
  EXPECT_ERROR ("(condition-case (signal 'cc-boom)"
                "  (cc-boom (signal 'cc-boom2))"
                "  (cc-boom2 'same))",
                boom2);

  // but by an outer one
  EXPECT_FORM ("(condition-case"
               "  (condition-case (signal 'cc-boom)"
               "    (cc-boom (signal 'cc-boom2)))"
               "  (cc-boom2 'outer))",
               "outer");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_condition_case_unwind ()
{
  // the cleanups run before the handler
  EXPECT_NO_ERROR_SRC ("(define cc-log nil)");
  EXPECT_NO_ERROR_SRC ("(condition-case"
                       "  (unwind-protect (signal 'cc-boom)"
                       "    (set! cc-log (cons 'cleanup cc-log)))"
                       "  (cc-boom (set! cc-log (cons 'handler cc-log))))");
  EXPECT_FORM ("cc-log", "(handler cleanup)");

  // rethrow: each cleanup runs once, the ones between the inner and the
  // outer condition-case too
  EXPECT_NO_ERROR_SRC ("(define cc-log nil)");
  EXPECT_NO_ERROR_SRC (
      "(condition-case"
      "  (unwind-protect"
      "    (condition-case"
      "      (unwind-protect (signal 'cc-boom)"
      "        (set! cc-log (cons 'inner cc-log)))"
      "      (cc-other 'other))"
      "    (set! cc-log (cons 'mid cc-log)))"
      "  (cc-boom (set! cc-log (cons 'handler cc-log))))");
  EXPECT_FORM ("cc-log", "(handler mid inner)");

  // a caught error does not run the cleanups outside the condition-case
  EXPECT_NO_ERROR_SRC ("(define cc-log nil)");
  EXPECT_NO_ERROR_SRC (
      "(unwind-protect"
      "  (progn"
      "    (condition-case (signal 'cc-boom) (cc-boom 'caught))"
      "    (set! cc-log (cons 'after cc-log)))"
      "  (set! cc-log (cons 'outer cc-log)))");
  EXPECT_FORM ("cc-log", "(outer after)");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_condition_case_handlers ()
{
  // more than HANDLERSIZE sequential condition-cases: every exit path must
  // pop its handler, or the handler stack overflows

  // local exit
  for (int i = 0; i < 600; i++)
    EXPECT_INT ("(condition-case 1 (cc-boom 0))", 1);

  // caught
  for (int i = 0; i < 600; i++)
    EXPECT_FORM ("(condition-case (signal 'cc-boom) (cc-boom 'ok))", "ok");

  // rethrown
  for (int i = 0; i < 600; i++)
    EXPECT_FORM ("(condition-case"
                 "  (condition-case (signal 'cc-boom) (cc-other 'inner))"
                 "  (cc-boom 'ok))",
                 "ok");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_condition_case_gc ()
{
  // the conses allocated after the gc try to reuse the blocks of what was
  // wrongly freed

  // rethrow: the error survives a gc in a cleanup between the inner and the
  // outer handler
  EXPECT_NO_ERROR_SRC ("(define cc-throw (lambda (a) (signal 'cc-boom a)))");
  Lisp_Object err;
  TEST_ASSERT (!eval_src ("(unwind-protect"
                          "  (condition-case (cc-throw (cons 3 4))"
                          "    (cc-other 'other))"
                          "  (gc)"
                          "  (cons (cons 0 0) (cons 0 0)))",
                          &err),
               "expected cc-boom, got no error");
  TEST_ASSERT (eq (f_error_symbol (err), f_intern (make_string ("cc-boom"))),
               "expected cc-boom, got %s", symname (f_error_symbol (err)));
  TEST_ASSERT (same_form (f_error_data (err), parse_src ("(3 . 4)")),
               "expected error data (3 . 4)");

  // the clauses survive a gc in the body
  EXPECT_FORM ("(condition-case"
               "  (progn (gc) (cons (cons 0 0) (cons 0 0)) (signal 'cc-boom))"
               "  (cc-other 'other)"
               "  (cc-boom (car (cons 'ok nil))))",
               "ok");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_append ()
{
  EXPECT_FORM ("(append '(1 2) '(3 4))", "(1 2 3 4)");
  EXPECT_FORM ("(append nil '(1))", "(1)");
  EXPECT_FORM ("(append '(1) nil)", "(1)");
  EXPECT_FORM ("(append nil nil)", "nil");
  // the last argument can be anything, like in other lisps
  EXPECT_FORM ("(append '(1) 2)", "(1 . 2)");

  // the first list is copied, the second is shared
  EXPECT_NO_ERROR_SRC ("(define app-l '(1 2))");
  EXPECT_NO_ERROR_SRC ("(define app-r '(3))");
  EXPECT_NO_ERROR_SRC ("(define app-res (append app-l app-r))");
  EXPECT_NO_ERROR_SRC ("(setcar app-res 9)");
  EXPECT_INT ("(car app-l)", 1);
  EXPECT_FORM ("(eq? (cdr (cdr app-res)) app-r)", "t");

  EXPECT_ERROR ("(append 1 '(2))", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_defmacro ()
{
  // args reach the macro unevaluated, then the expansion is evaluated
  EXPECT_NO_ERROR_SRC ("(defmacro (mac-if c a b)"
                       "  (cons 'if (cons c (cons a (cons b nil)))))");
  EXPECT_INT ("(mac-if t 1 2)", 1);
  EXPECT_INT ("(mac-if nil 1 2)", 2);
  // only the taken branch is evaluated, the other one would fail
  EXPECT_INT ("(mac-if t 1 (car 1))", 1);

  EXPECT_NO_ERROR_SRC ("(defmacro (mac-quote x) (cons 'quote (cons x nil)))");
  EXPECT_FORM ("(mac-quote (a b))", "(a b)");

  // no args
  EXPECT_NO_ERROR_SRC ("(defmacro (mac-zero) 42)");
  EXPECT_INT ("(mac-zero)", 42);

  // nested uses, and a macro expanding into another macro
  EXPECT_INT ("(mac-if (mac-if t nil t) 1 2)", 2);
  EXPECT_NO_ERROR_SRC ("(defmacro (mac-not x)"
                       "  (cons 'mac-if (cons x (cons nil (cons t nil)))))");
  EXPECT_FORM ("(mac-not nil)", "t");

  // uses in a function body are expanded at definition
  EXPECT_NO_ERROR_SRC ("(define (mac-user x) (mac-if x 10 20))");
  EXPECT_INT ("(mac-user nil)", 20);

  // eval expands too
  EXPECT_INT ("(eval '(mac-if t 1 2))", 1);

  // quoted data is not expanded
  EXPECT_FORM ("'(mac-if t 1 2)", "(mac-if t 1 2)");

  // macroexpand-1 expands one step, only if the head is a macro
  EXPECT_FORM ("(macroexpand-1 '(mac-if t 1 2))", "(if t 1 2)");
  EXPECT_FORM ("(macroexpand-1 '(mac-not x))", "(mac-if x nil t)");
  EXPECT_FORM ("(macroexpand-1 '(car x))", "(car x)");
  EXPECT_INT ("(macroexpand-1 1)", 1);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_defmacro_error ()
{
  EXPECT_NO_ERROR_SRC ("(defmacro (mac-two a b) a)");
  // the macro lambda checks its arity at expansion
  EXPECT_ERROR ("(mac-two 1)", q_error_funcargs);
  // a macro is not a function, it cannot be applied at runtime
  EXPECT_ERROR ("(apply mac-two '(1 2))", q_error_funcinvalid);

  // the signature must be a list starting with a symbol
  EXPECT_ERROR ("(defmacro mac-bad 1)", q_error_type);
  EXPECT_ERROR ("(defmacro (1 x) x)", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_quasiquote ()
{
  EXPECT_NO_ERROR_SRC ("(define qq-x 42)");
  EXPECT_NO_ERROR_SRC ("(define qq-l '(1 2))");

  // the reader
  EXPECT_FORM ("'`(a ,b ,@c)",
               "(quasiquote (a (unquote b) (unquote-splicing c)))");

  // without unquotes it is like quote
  EXPECT_FORM ("`a", "a");
  EXPECT_INT ("`1", 1);
  EXPECT_FORM ("`()", "nil");
  EXPECT_FORM ("`(a b)", "(a b)");

  // unquote
  EXPECT_INT ("`,qq-x", 42);
  EXPECT_FORM ("`(a ,qq-x)", "(a 42)");
  EXPECT_FORM ("`(a ,(+ qq-x 1))", "(a 43)");
  EXPECT_FORM ("`((a ,qq-x) b)", "((a 42) b)");
  EXPECT_FORM ("`(a . ,qq-x)", "(a . 42)");

  // unquote-splicing
  EXPECT_FORM ("`(a ,@qq-l b)", "(a 1 2 b)");
  EXPECT_FORM ("`(,@qq-l)", "(1 2)");
  EXPECT_FORM ("`(,@qq-l b)", "(1 2 b)");
  EXPECT_FORM ("`(a ,@nil b)", "(a b)");
  EXPECT_FORM ("`(a ,@qq-l ,qq-x)", "(a 1 2 42)");

  // the spliced list is copied
  EXPECT_NO_ERROR_SRC ("(define qq-r `(,@qq-l 3))");
  EXPECT_NO_ERROR_SRC ("(setcar qq-r 9)");
  EXPECT_INT ("(car qq-l)", 1);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_quasiquote_macro ()
{
  // macros written with backquote
  EXPECT_NO_ERROR_SRC ("(defmacro (qq-swap a b) `(cons ,b ,a))");
  EXPECT_FORM ("(qq-swap 1 2)", "(2 . 1)");

  EXPECT_NO_ERROR_SRC ("(defmacro (qq-unless c body) `(if ,c nil ,body))");
  EXPECT_INT ("(qq-unless nil 5)", 5);
  EXPECT_FORM ("(qq-unless t (car 1))", "nil");

  // splicing a list of forms into the expansion
  EXPECT_NO_ERROR_SRC ("(defmacro (qq-progn forms) `(progn ,@forms))");
  EXPECT_INT ("(qq-progn (1 2 3))", 3);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_lambda_rest ()
{
  // the extra args are collected in a list bound to the rest symbol
  EXPECT_FORM ("((lambda (a . r) r) 1)", "nil");
  EXPECT_FORM ("((lambda (a . r) r) 1 2 3)", "(2 3)");
  EXPECT_INT ("((lambda (a . r) a) 1 2 3)", 1);
  EXPECT_FORM ("((lambda (a b . r) (cons b r)) 1 2 3 4)", "(2 3 4)");

  // a symbol instead of a list takes all the args
  EXPECT_FORM ("((lambda r r))", "nil");
  EXPECT_FORM ("((lambda r r) 1 2 3)", "(1 2 3)");

  // the args are evaluated before being collected
  EXPECT_FORM ("((lambda (a . r) r) 1 (+ 1 1) (car '(3)))", "(2 3)");

  // apply spreads the list on required and rest args
  EXPECT_FORM ("(apply (lambda (a . r) r) '(1 2 3))", "(2 3)");
  EXPECT_FORM ("(apply (lambda r r) '(1 2))", "(1 2)");
  EXPECT_FORM ("(apply (lambda r r) nil)", "nil");

  // the rest list is fresh, not the list given to apply
  EXPECT_NO_ERROR_SRC ("(define rest-l '(1 2))");
  EXPECT_NO_ERROR_SRC ("(apply (lambda r (setcar r 9)) rest-l)");
  EXPECT_INT ("(car rest-l)", 1);

  // a closure captures the rest arg like any other arg
  EXPECT_NO_ERROR_SRC ("(define rest-mk (lambda r (lambda () r)))");
  EXPECT_FORM ("((rest-mk 1 2))", "(1 2)");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_define_rest ()
{
  // (define (f a . r) body) is (define f (lambda (a . r) body))
  EXPECT_NO_ERROR_SRC ("(define (def-rest-f a . r) (cons a r))");
  EXPECT_FORM ("(def-rest-f 1)", "(1)");
  EXPECT_FORM ("(def-rest-f 1 2 3)", "(1 2 3)");

  // (define (f . r) body) is (define f (lambda r body))
  EXPECT_NO_ERROR_SRC ("(define (def-rest-all . r) r)");
  EXPECT_FORM ("(def-rest-all)", "nil");
  EXPECT_FORM ("(def-rest-all 1 2)", "(1 2)");

  // recursion on the rest list
  EXPECT_NO_ERROR_SRC ("(define (def-rest-sum . r)"
                       "  (if r (+ (car r) (apply def-rest-sum (cdr r))) 0))");
  EXPECT_INT ("(def-rest-sum)", 0);
  EXPECT_INT ("(def-rest-sum 1 2 3 4)", 10);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_defmacro_rest ()
{
  // the rest arg collects the unevaluated forms
  EXPECT_NO_ERROR_SRC ("(defmacro (mac-rest-when c . body)"
                       "  `(if ,c (progn ,@body) nil))");
  EXPECT_INT ("(mac-rest-when t 1 2 3)", 3);
  EXPECT_FORM ("(mac-rest-when nil (car 1))", "nil");
  EXPECT_FORM ("(macroexpand-1 '(mac-rest-when c a b))",
               "(if c (progn a b) nil)");

  EXPECT_NO_ERROR_SRC ("(defmacro (mac-rest-list . xs) `(quote ,xs))");
  EXPECT_FORM ("(mac-rest-list a b)", "(a b)");
  EXPECT_FORM ("(mac-rest-list)", "nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_lambda_rest_error ()
{
  // the required args are still required
  EXPECT_ERROR ("((lambda (a b . r) a) 1)", q_error_funcargs);
  EXPECT_ERROR ("(apply (lambda (a . r) a) nil)", q_error_funcargs);
  EXPECT_NO_ERROR_SRC ("(define (def-rest-req a . r) a)");
  EXPECT_ERROR ("(def-rest-req)", q_error_funcargs);

  // the rest arg must be a symbol
  EXPECT_ERROR ("(lambda (a . 1) a)", q_error_type);
  EXPECT_ERROR ("(lambda 1 1)", q_error_type);
  EXPECT_ERROR ("(define (def-rest-bad a . 1) a)", q_error_type);
  // the required args too
  EXPECT_ERROR ("(lambda (a 1 . r) a)", q_error_type);
  return TEST_RESULT_SUCCESS;
}
