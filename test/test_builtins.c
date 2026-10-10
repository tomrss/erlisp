#include "../src/alloc.h"
#include "../src/eval.h"
#include "../src/lexer.h"
#include "../src/lisp.h"
#include "../src/parser.h"
#include "test_lib.h"
#include <stdio.h>
#include <string.h>

// helpers

static Lisp_Object parse_src (const char *src);
static int eval_src (const char *src, Lisp_Object *out);
static int same_form (Lisp_Object x, Lisp_Object y);
static const char *symname (Lisp_Object symbol);

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
static TestResult test_builtins_load_global ();
static TestResult test_builtins_load_error ();
static TestResult test_builtins_read ();

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
  { .skip = 0, .name = "load global", .run = test_builtins_load_global },
  { .skip = 0, .name = "load error", .run = test_builtins_load_error },
  { .skip = 0, .name = "read", .run = test_builtins_read },
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
  Lexer *l = lex_init (stream_string (src, strlen (src)));
  Lisp_Object form;
  if (!parse_next_sexp (l, &form))
    form = q_nil;
  lex_close (l);
  return form;
}

static int
eval_src (const char *src, Lisp_Object *out)
{
  return safe_eval (l_globalenv, parse_src (src), out);
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
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_multiply ()
{
  EXPECT_INT ("(*)", 1);
  EXPECT_INT ("(* 2 3 4)", 24);
  EXPECT_INT ("(* 2 0)", 0);
  EXPECT_ERROR ("(* 2 \"a\")", q_error_type);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_builtins_divide ()
{
  EXPECT_INT ("(/ 20 2 5)", 2);
  EXPECT_INT ("(/ 7 2)", 3);
  EXPECT_ERROR ("(/ 1 0)", q_error_arith);
  EXPECT_ERROR ("(/ 1 \"a\")", q_error_type);
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
  EXPECT_INT ("(read \"42\")", 42);
  EXPECT_FORM ("(read \"(a (b . c) 1)\")", "(a (b . c) 1)");
  EXPECT_INT ("(eval (read \"(+ 1 2)\"))", 3);
  // syntax errors are signaled, not fatal
  EXPECT_ERROR ("(read \"(1 2\")", q_error_syntax);
  EXPECT_ERROR ("(read \")\")", q_error_syntax);
  EXPECT_ERROR ("(read 1)", q_error_type);
  return TEST_RESULT_SUCCESS;
}
