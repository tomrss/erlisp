#include "../src/alloc.h"
#include "../src/debug.h"
#include "../src/env.h"
#include "../src/eval.h"
#include "../src/lisp.h"
#include "test_lib.h"
#include <stdio.h>

// test cases
static TestResult test_eval_symbol ();
static TestResult test_eval_vector ();
static TestResult test_eval_string ();
static TestResult test_eval_int ();
static TestResult test_eval_nil ();
static TestResult test_eval_t ();
static TestResult test_eval_subr_equal ();
static TestResult test_eval_subr_strlen ();
static TestResult test_eval_lambda_2args ();
static TestResult test_eval_lambda_nested ();
static TestResult test_eval_progn ();
static TestResult test_eval_progn_single ();
static TestResult test_eval_quote ();

static TestCase test_eval_cases[] = {
  { .skip = 0, .name = "symbol", .run = test_eval_symbol },
  { .skip = 0, .name = "vector", .run = test_eval_vector },
  { .skip = 0, .name = "string", .run = test_eval_string },
  { .skip = 0, .name = "int", .run = test_eval_int },
  { .skip = 0, .name = "nil", .run = test_eval_nil },
  { .skip = 0, .name = "t", .run = test_eval_t },
  { .skip = 0, .name = "subr equal", .run = test_eval_subr_equal },
  { .skip = 0, .name = "subr strlen", .run = test_eval_subr_strlen },
  { .skip = 0, .name = "lambda 2args", .run = test_eval_lambda_2args },
  { .skip = 0, .name = "lambda nested", .run = test_eval_lambda_nested },
  { .skip = 0, .name = "progn", .run = test_eval_progn },
  { .skip = 0, .name = "progn single", .run = test_eval_progn_single },
  { .skip = 0, .name = "quote", .run = test_eval_quote },
  {}, // terminator
};

TestSuite *
test_suite_eval ()
{
  return test_suite_init ("eval", test_eval_cases);
}

// test cases implementation

static TestResult
test_eval_symbol ()
{
  Lisp_Object test = make_nstr_symbol ("mysymb", 6);
  unbox_symbol (test)->value = box_int (12);
  Lisp_Object res = eval (l_globalenv, test);
  TEST_CHECK_TYPE ("eval symbol", res, LISP_INTG);
  Lisp_Integer ures = unbox_int (res);
  if (ures != 12)
    return TEST_RESULT_FAIL ("expected symb to eval to 12, got %ld", ures);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_vector ()
{
  Lisp_Object test = make_vector (10);
  Lisp_Object res = eval (l_globalenv, test);
  TEST_CHECK_TYPE ("eval vect", res, LISP_VECT);
  if (!eq (test, res))
    return TEST_RESULT_FAIL ("expected vect to eval to itself");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_string ()
{
  Lisp_Object test = make_string ("test");
  Lisp_Object res = eval (l_globalenv, test);
  TEST_CHECK_TYPE ("eval string", res, LISP_STRG);
  if (!eq (test, res))
    return TEST_RESULT_FAIL ("expected string to eval to itself");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_int ()
{
  Lisp_Object test = box_int (-999);
  Lisp_Object res = eval (l_globalenv, test);
  TEST_CHECK_TYPE ("eval int", res, LISP_INTG);
  Lisp_Integer ures = unbox_int (res);
  if (!eq (test, res))
    return TEST_RESULT_FAIL ("expected int to eval to itself");
  if (ures != -999)
    return TEST_RESULT_FAIL ("expected %ld, got %ld", -999, ures);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_nil ()
{
  Lisp_Object res = eval (l_globalenv, q_nil);
  TEST_ASSERT (eq(res, q_nil), "expected nil to eval to nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_t ()
{
  Lisp_Object res = eval (l_globalenv, q_t);
  TEST_ASSERT (eq(res, q_t), "expected t to eval to t");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_subr_equal ()
{
  Lisp_Object subr = make_subr ("equal", 2, 2, NSUBR (2, f_equal_p));
  Lisp_Object subrsymb = make_nstr_symbol ("equal", 5);
  unbox_symbol (subrsymb)->value = subr;
  Lisp_Object test = make_cons (
      subrsymb, make_cons (box_int (1), make_cons (box_int (2), q_nil)));
  Lisp_Object res = eval (l_globalenv, test);
  if (!eq (res, q_nil))
    return TEST_RESULT_FAIL ("expect (equal 1 2) to evaluate to nil");
  test = make_cons (subrsymb,
                    make_cons (box_int (1), make_cons (box_int (1), q_nil)));
  res = eval (l_globalenv, test);
  if (!eq (res, q_t))
    return TEST_RESULT_FAIL ("expect (equal 1 1) to evaluate to t");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_subr_strlen ()
{
  Lisp_Object subr = make_subr ("string-length", 1, 1, NSUBR (1, f_string_length));
  Lisp_Object subrsymb = make_str_symbol ("string-length");
  Lisp_Object teststr = make_nstring ("test", 4);
  unbox_symbol (subrsymb)->value = subr;
  Lisp_Object test = make_cons (subrsymb, make_cons (teststr, q_nil));
  Lisp_Object res = eval (l_globalenv, test);
  TEST_CHECK_TYPE ("string-length res", res, LISP_INTG);

  if (unbox_int (res) != 4)
    return TEST_RESULT_FAIL ("expect strlen to evaluate to 4, got %ld",
                             unbox_int (res));
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_lambda_2args ()
{
  /*
    Define a lambda call like this:

    (lambda (arg1 arg2)
      (cons "not return" "this")
      (+ arg1 arg2)) (1 2)

      ---> 3
   */
  Lisp_Object env, form;
  Lisp_Object lambda, lambdasym, lambdabody;
  Lisp_Object arg1, arg2;
  Lisp_Object arg1val, arg2val;
  Lisp_Object subrsum, subrcons;
  Lisp_Object args[2];

  args[0] = arg1 = make_str_symbol ("arg1");
  args[1] = arg2 = make_str_symbol ("arg2");

  arg1val = box_int (1);
  arg2val = box_int (2);

  subrsum = DEFSUBR ("+", 0, MANY, f_sum);
  subrcons = DEFSUBR ("cons", 2, 2, f_cons);

  // bind arg1 in parent env to verify lexical scope
  Lisp_Object parentarg1 = make_str_symbol ("arg1");
  unbox_symbol (parentarg1)->value = box_int (998);
  env = env_new (l_globalenv, parentarg1);

  lambdabody = f_cons (
      f_cons (subrcons, f_cons (make_string ("not return"),
                                f_cons (make_string ("this"), q_nil))),
      f_cons (f_cons (subrsum, f_cons (arg1, f_cons (arg2, q_nil))), q_nil));
  lambda = make_lambda (2, 2, args, lambdabody);
  lambdasym = make_str_symbol ("something");
  unbox_symbol (lambdasym)->value = lambda;

  form = f_cons (lambdasym, f_cons (arg1val, f_cons (arg2val, q_nil)));

  Lisp_Object result = eval (env, form);

  TEST_CHECK_TYPE ("result", result, LISP_INTG);
  TEST_ASSERT (unbox_int (result) == 3, "expected %d, got %ld", 3,
               unbox_int (result));

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_lambda_nested ()
{
  /*
    Define a lambda call like this:

    (define (fun1 arg1 arg2)
      (+ arg1 (string-length arg2))

    (define (fun2 arg1)
      (fun1 3 arg1))

    (fun2 "test")
      ---> 7
   */
  Lisp_Object form;
  Lisp_Object fun1lambda, fun1body, fun1sym;
  Lisp_Object fun2lambda, fun2body, fun2sym;
  Lisp_Object fun1arg1, fun1arg2;
  Lisp_Object fun2arg1;
  Lisp_Object fun2arg1val;
  Lisp_Object subrsum, subrstrlen;
  Lisp_Object fun1args[2];
  Lisp_Object fun2args[1];

  fun1args[0] = fun1arg1 = make_str_symbol ("arg1");
  fun1args[1] = fun1arg2 = make_str_symbol ("arg2");
  fun2args[0] = fun2arg1 = make_str_symbol ("arg1");

  fun2arg1val = make_string ("test");

  subrsum = DEFSUBR ("+", 0, MANY, f_sum);
  subrstrlen = DEFSUBR ("string-length", 1, 1, f_string_length);

  Lisp_Object callstrlen = f_cons (subrstrlen, f_cons (fun1arg2, q_nil));
  fun1body = f_cons (
      f_cons (subrsum, f_cons (fun1arg1, f_cons (callstrlen, q_nil))), q_nil);
  fun1lambda = make_lambda (2, 2, fun1args, fun1body);
  fun1sym = make_str_symbol ("fun1");
  unbox_symbol (fun1sym)->value = fun1lambda;

  fun2body = f_cons (
      f_cons (fun1sym, f_cons (box_int (3), f_cons (fun2arg1, q_nil))), q_nil);
  fun2lambda = make_lambda (1, 1, fun2args, fun2body);
  fun2sym = make_str_symbol ("fun2");
  unbox_symbol (fun2sym)->value = fun2lambda;

  form = f_cons (fun2sym, f_cons (fun2arg1val, q_nil));

  Lisp_Object result = eval (l_globalenv, form);

  TEST_CHECK_TYPE ("result", result, LISP_INTG);
  TEST_ASSERT (unbox_int (result) == 7, "expected %d, got %ld", 7,
               unbox_int (result));

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_progn ()
{
  Lisp_Object form
      = f_cons (make_string ("test"),
                f_cons (f_cons (DEFSUBR ("strlen", 1, 1, f_string_length),
                                f_cons (make_string ("test"), q_nil)),
                        q_nil));

  Lisp_Object result = progn (l_globalenv, form);
  TEST_ASSERT (eq (result, box_int (4)), "wrong");

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_progn_single ()
{
  Lisp_Object form = f_cons (f_cons (DEFSUBR ("strlen", 1, 1, f_string_length),
                                     f_cons (make_string ("test"), q_nil)),
                             q_nil);

  Lisp_Object result = progn (l_globalenv, form);
  debug_print_form (result);
  TEST_ASSERT (eq (result, box_int (4)), "wrong");

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_quote ()
{
  Lisp_Object form
      = f_cons (DEFSUBR ("quote", 1, UNEVALLED, f_quote),
                f_cons (f_cons (DEFSUBR ("strlen", 1, 1, f_string_length),
                                f_cons (make_string ("test"), q_nil)),
                        q_nil));

  Lisp_Object result = progn (l_globalenv, form);
  debug_print_form (result);
  TEST_CHECK_TYPE ("quoted", form, LISP_CONS);

  return TEST_RESULT_SUCCESS;
}
