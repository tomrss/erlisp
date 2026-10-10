#include "../src/alloc.h"
#include "../src/print.h"
#include "../src/env.h"
#include "../src/eval.h"
#include "../src/lisp.h"
#include "../src/obarray.h"
#include "test_lib.h"
#include <stdarg.h>
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
static TestResult test_eval_load ();
static TestResult test_eval_signal ();
static TestResult test_eval_lexical_closure ();
static TestResult test_eval_lexical_caller_locals ();
static TestResult test_eval_lexical_counter ();
static TestResult test_eval_setq_scopes ();
static TestResult test_eval_setq_unbound ();
static TestResult test_eval_lexical_inner_define ();
static TestResult test_eval_lexical_gc_caller_locals ();
static TestResult test_eval_lexical_head_eval ();

// name of the error symbol, for failure messages
static const char *errname (Lisp_Object err);
// interned symbol by name, e.g. the symbol of a builtin
static Lisp_Object sym (const char *name);
// lisp list of n elements, like (list ...)
static Lisp_Object mklist (int n, ...);

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
  { .skip = 0, .name = "load", .run = test_eval_load },
  { .skip = 0, .name = "signal", .run = test_eval_signal },
  { .skip = 0, .name = "lex closure", .run = test_eval_lexical_closure },
  { .skip = 0, .name = "lex locals", .run = test_eval_lexical_caller_locals },
  { .skip = 0, .name = "lex counter", .run = test_eval_lexical_counter },
  { .skip = 0, .name = "set! scopes", .run = test_eval_setq_scopes },
  { .skip = 0, .name = "set! unbound", .run = test_eval_setq_unbound },
  { .skip = 0,
    .name = "lex inner def",
    .run = test_eval_lexical_inner_define },
  { .skip = 0,
    .name = "lex gc locals",
    .run = test_eval_lexical_gc_caller_locals },
  { .skip = 0, .name = "lex head eval", .run = test_eval_lexical_head_eval },
  {}, // terminator
};

TestSuite *
test_suite_eval ()
{
  return test_suite_init ("eval", test_eval_cases);
}

static const char *
errname (Lisp_Object err)
{
  Lisp_Object symbol = f_error_symbol (err);
  if (type_of (symbol) != LISP_SYMB)
    return "<not a symbol>";
  const char *name = unbox_string (unbox_symbol (symbol)->name)->data;

  // for unbound-error, data is (symbol): show which symbol is unbound
  Lisp_Object data = f_error_data (err);
  if (eq (symbol, q_error_unbound) && type_of (data) == LISP_CONS
      && type_of (f_car (data)) == LISP_SYMB)
    return test_real_sprintf (
        "%s: %s", name,
        unbox_string (unbox_symbol (f_car (data))->name)->data);

  // for type-error, data is (expected... provided obj) with type names as
  // strings: show expected types (joined with |) and provided type. the
  // last element is the object itself, skipped as it can be a string too
  if (eq (symbol, q_error_type))
    {
      const char *expected = "";
      const char *provided = NULL;
      for (Lisp_Object tail = data;
           type_of (tail) == LISP_CONS && type_of (f_cdr (tail)) == LISP_CONS
           && type_of (f_car (tail)) == LISP_STRG;
           tail = f_cdr (tail))
        {
          if (provided)
            expected = *expected
                           ? test_real_sprintf ("%s|%s", expected, provided)
                           : provided;
          provided = unbox_string (f_car (tail))->data;
        }
      if (provided && *expected)
        return test_real_sprintf ("%s %s %s", name, expected, provided);
    }

  return name;
}

static Lisp_Object
sym (const char *name)
{
  return f_intern (make_string (name));
}

#define MKLIST_MAX 10

static Lisp_Object
mklist (int n, ...)
{
  Lisp_Object elts[MKLIST_MAX];
  va_list ap;
  va_start (ap, n);
  for (int i = 0; i < n; i++)
    elts[i] = va_arg (ap, Lisp_Object);
  va_end (ap);

  Lisp_Object list = q_nil;
  for (int i = n - 1; i >= 0; i--)
    list = f_cons (elts[i], list);
  return list;
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
  TEST_ASSERT (eq (res, q_nil), "expected nil to eval to nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_t ()
{
  Lisp_Object res = eval (l_globalenv, q_t);
  TEST_ASSERT (eq (res, q_t), "expected t to eval to t");
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
  Lisp_Object res;
  TEST_ASSERT (safe_eval (l_globalenv, test, &res), "unexpected %s",
               errname (res));
  if (!eq (res, q_nil))
    return TEST_RESULT_FAIL ("expect (equal 1 2) to evaluate to nil");
  test = make_cons (subrsymb,
                    make_cons (box_int (1), make_cons (box_int (1), q_nil)));
  TEST_ASSERT (safe_eval (l_globalenv, test, &res), "unexpected %s",
               errname (res));
  if (!eq (res, q_t))
    return TEST_RESULT_FAIL ("expect (equal 1 1) to evaluate to t");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_subr_strlen ()
{
  Lisp_Object subr
      = make_subr ("string-length", 1, 1, NSUBR (1, f_string_length));
  Lisp_Object subrsymb = make_str_symbol ("string-length");
  Lisp_Object teststr = make_nstring ("test", 4);
  unbox_symbol (subrsymb)->value = subr;
  Lisp_Object test = make_cons (subrsymb, make_cons (teststr, q_nil));
  Lisp_Object res;
  TEST_ASSERT (safe_eval (l_globalenv, test, &res), "unexpected %s",
               errname (res));
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
  // make_lambda does not mark args as local bound, f_lambda does
  unbox_symbol (arg1)->localbound = 1;
  unbox_symbol (arg2)->localbound = 1;

  arg1val = box_int (1);
  arg2val = box_int (2);

  subrsum = DEFSUBR ("+", 0, MANY, f_sum);
  subrcons = DEFSUBR ("cons", 2, 2, f_cons);

  // bind arg1 in parent env to verify the lambda arg shadows it
  env = env_new (l_globalenv);
  env_define (env, arg1, box_int (998));

  lambdabody = f_cons (
      f_cons (subrcons, f_cons (make_string ("not return"),
                                f_cons (make_string ("this"), q_nil))),
      f_cons (f_cons (subrsum, f_cons (arg1, f_cons (arg2, q_nil))), q_nil));
  lambda = make_lambda (2, 2, env, args, lambdabody);
  lambdasym = make_str_symbol ("something");
  unbox_symbol (lambdasym)->value = lambda;

  form = f_cons (lambdasym, f_cons (arg1val, f_cons (arg2val, q_nil)));

  Lisp_Object result;
  TEST_ASSERT (safe_eval (env, form, &result), "unexpected %s",
               errname (result));

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
  // make_lambda does not mark args as local bound, f_lambda does
  unbox_symbol (fun1arg1)->localbound = 1;
  unbox_symbol (fun1arg2)->localbound = 1;
  unbox_symbol (fun2arg1)->localbound = 1;

  fun2arg1val = make_string ("test");

  subrsum = DEFSUBR ("+", 0, MANY, f_sum);
  subrstrlen = DEFSUBR ("string-length", 1, 1, f_string_length);

  Lisp_Object callstrlen = f_cons (subrstrlen, f_cons (fun1arg2, q_nil));
  fun1body = f_cons (
      f_cons (subrsum, f_cons (fun1arg1, f_cons (callstrlen, q_nil))), q_nil);
  fun1lambda = make_lambda (2, 2, env_current (), fun1args, fun1body);
  fun1sym = make_str_symbol ("fun1");
  unbox_symbol (fun1sym)->value = fun1lambda;

  fun2body = f_cons (
      f_cons (fun1sym, f_cons (box_int (3), f_cons (fun2arg1, q_nil))), q_nil);
  fun2lambda = make_lambda (1, 1, env_current (), fun2args, fun2body);
  fun2sym = make_str_symbol ("fun2");
  unbox_symbol (fun2sym)->value = fun2lambda;

  form = f_cons (fun2sym, f_cons (fun2arg1val, q_nil));

  Lisp_Object result;
  TEST_ASSERT (safe_eval (l_globalenv, form, &result), "unexpected %s",
               errname (result));

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

  Lisp_Object result;
  TEST_ASSERT (condition_case_2 (progn, l_globalenv, form, &result),
               "unexpected %s", errname (result));
  TEST_ASSERT (eq (result, box_int (4)), "wrong");

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_progn_single ()
{
  Lisp_Object form = f_cons (f_cons (DEFSUBR ("strlen", 1, 1, f_string_length),
                                     f_cons (make_string ("test"), q_nil)),
                             q_nil);

  Lisp_Object result;
  TEST_ASSERT (condition_case_2 (progn, l_globalenv, form, &result),
               "unexpected %s", errname (result));
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

  Lisp_Object result;
  TEST_ASSERT (condition_case_2 (progn, l_globalenv, form, &result),
               "unexpected %s", errname (result));
  debug_print_form (result);
  TEST_CHECK_TYPE ("quoted", form, LISP_CONS);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_load ()
{
  // interned, so the parser resolves load-test-cell to this symbol
  Lisp_Object cell = make_cons (box_int (0), q_nil);
  Lisp_Object symb = make_str_symbol ("load-test-cell");
  unbox_symbol (symb)->value = cell;
  obarray_put (v_obarray, symb);

  Lisp_Object res;
  TEST_ASSERT (
      condition_case_1 (f_load, make_string ("test/assets/src-load.tl"), &res),
      "unexpected %s", errname (res));
  TEST_ASSERT (eq (res, q_t), "expected load to return t");

  // the second form reads the value set by the first one
  Lisp_Object car = f_car (cell);
  TEST_CHECK_TYPE ("load-test-cell car", car, LISP_INTG);
  if (unbox_int (car) != 42)
    return TEST_RESULT_FAIL ("expected car to be 42, got %ld",
                             unbox_int (car));

  // the define in the loaded file is global: the value is in the symbol,
  // interned by the parser
  Lisp_Object defsymb
      = obarray_lookup_name (v_obarray, make_string ("load-test-define"));
  TEST_CHECK_TYPE ("load-test-define symb", defsymb, LISP_SYMB);
  Lisp_Object defval = unbox_symbol (defsymb)->value;
  TEST_CHECK_TYPE ("load-test-define value", defval, LISP_INTG);
  if (unbox_int (defval) != 7)
    return TEST_RESULT_FAIL ("expected load-test-define to be 7, got %ld",
                             unbox_int (defval));

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_signal ()
{
  /*
    (progn
      (define f1 (lambda () (signal 'err1 "my error")))
      (define f2 (lambda () (f1)))
      (f2))
   */
  Lisp_Object subrprogn
      = obarray_lookup_name (v_obarray, make_string ("progn"));
  Lisp_Object subrquote
      = obarray_lookup_name (v_obarray, make_string ("quote"));
  Lisp_Object subrlambda
      = obarray_lookup_name (v_obarray, make_string ("lambda"));
  Lisp_Object subrdefine
      = obarray_lookup_name (v_obarray, make_string ("define"));
  Lisp_Object subrsignal
      = obarray_lookup_name (v_obarray, make_string ("signal"));

  Lisp_Object f1sym = make_str_symbol ("f1");
  Lisp_Object f2sym = make_str_symbol ("f2");
  Lisp_Object err1sym = make_str_symbol ("err1");

  // (signal 'err1 "my error")
  Lisp_Object callsignal = f_cons (
      subrsignal, f_cons (f_cons (subrquote, f_cons (err1sym, q_nil)),
                          f_cons (make_string ("my error"), q_nil)));
  // (define f1 (lambda () (signal 'err1 "my error")))
  Lisp_Object definef1 = f_cons (
      subrdefine,
      f_cons (f1sym,
              f_cons (f_cons (subrlambda,
                              f_cons (q_nil, f_cons (callsignal, q_nil))),
                      q_nil)));
  // (define f2 (lambda () (f1)))
  Lisp_Object definef2 = f_cons (
      subrdefine,
      f_cons (f2sym,
              f_cons (f_cons (subrlambda,
                              f_cons (q_nil,
                                      f_cons (f_cons (f1sym, q_nil), q_nil))),
                      q_nil)));

  Lisp_Object form = f_cons (
      subrprogn,
      f_cons (definef1,
              f_cons (definef2, f_cons (f_cons (f2sym, q_nil), q_nil))));

  Lisp_Object err;
  int success = condition_case_2 (eval, l_globalenv, form, &err);
  TEST_ASSERT (!success, "expected form to signal error");
  TEST_CHECK_TYPE ("error", err, LISP_CONS);

  TEST_ASSERT (eq (f_error_symbol (err), err1sym),
               "expected error symbol to be err1");

  // frames are collected from the innermost one
  const char *expected[] = { "signal", "f1", "f2", "progn" };
  int nexpected = sizeof (expected) / sizeof (expected[0]);

  Lisp_Object backtrace = f_error_backtrace (err);
  TEST_CHECK_TYPE ("backtrace", backtrace, LISP_CONS);
  TEST_ASSERT (unbox_int (f_length (backtrace)) == nexpected,
               "expected backtrace of %d frames, got %ld", nexpected,
               unbox_int (f_length (backtrace)));

  Lisp_Object tail = backtrace;
  for (int i = 0; i < nexpected; i++)
    {
      Lisp_Object frame = f_car (tail);
      TEST_CHECK_TYPE ("backtrace frame", frame, LISP_STRG);
      TEST_ASSERT (
          eq (f_string_equal_p (frame, make_string (expected[i])), q_t),
          "expected frame %d to be '%s', got '%s'", i, expected[i],
          unbox_string (frame)->data);
      tail = f_cdr (tail);
    }

  Lisp_Object data = f_error_data (err);
  TEST_CHECK_TYPE ("data", data, LISP_STRG);
  TEST_ASSERT (eq (f_string_equal_p (data, make_string ("my error")), q_t),
               "expected data to be 'my error', got '%s'",
               unbox_string (data)->data);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_lexical_closure ()
{
  /*
    A closure keeps the env of its definition after the outer call returns:

    (progn
      (define make-adder (lambda (n) (lambda (x) (+ x n))))
      (define add5 (make-adder 5))
      (add5 10))
      ---> 15
   */
  Lisp_Object makeadder = make_str_symbol ("make-adder");
  Lisp_Object add5 = make_str_symbol ("add5");
  Lisp_Object n = make_str_symbol ("n");
  Lisp_Object x = make_str_symbol ("x");

  Lisp_Object form = mklist (
      4, sym ("progn"),
      mklist (3, sym ("define"), makeadder,
              mklist (3, sym ("lambda"), mklist (1, n),
                      mklist (3, sym ("lambda"), mklist (1, x),
                              mklist (3, sym ("+"), x, n)))),
      mklist (3, sym ("define"), add5, mklist (2, makeadder, box_int (5))),
      mklist (2, add5, box_int (10)));

  Lisp_Object res;
  TEST_ASSERT (safe_eval (l_globalenv, form, &res), "unexpected %s",
               errname (res));
  TEST_CHECK_TYPE ("result", res, LISP_INTG);
  TEST_ASSERT (unbox_int (res) == 15, "expected %d, got %ld", 15,
               unbox_int (res));

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_lexical_caller_locals ()
{
  /*
    The locals of the caller are not visible in the callee:

    (progn
      (define y 0)
      (define get-y (lambda () y))
      (define call-with-y (lambda (y) (get-y)))
      (call-with-y 1))
      ---> 0 (dynamic scope would give 1)

    (progn
      (define get-z (lambda () z))
      (define call-with-z (lambda (z) (get-z)))
      (call-with-z 1))
      ---> unbound-error
   */
  Lisp_Object y = make_str_symbol ("y");
  Lisp_Object gety = make_str_symbol ("get-y");
  Lisp_Object callwithy = make_str_symbol ("call-with-y");

  Lisp_Object form = mklist (
      5, sym ("progn"), mklist (3, sym ("define"), y, box_int (0)),
      mklist (3, sym ("define"), gety, mklist (3, sym ("lambda"), q_nil, y)),
      mklist (3, sym ("define"), callwithy,
              mklist (3, sym ("lambda"), mklist (1, y), mklist (1, gety))),
      mklist (2, callwithy, box_int (1)));

  Lisp_Object res;
  TEST_ASSERT (safe_eval (l_globalenv, form, &res), "unexpected %s",
               errname (res));
  TEST_CHECK_TYPE ("result", res, LISP_INTG);
  TEST_ASSERT (unbox_int (res) == 0, "expected %d, got %ld", 0,
               unbox_int (res));

  Lisp_Object z = make_str_symbol ("z");
  Lisp_Object getz = make_str_symbol ("get-z");
  Lisp_Object callwithz = make_str_symbol ("call-with-z");

  form = mklist (
      4, sym ("progn"),
      mklist (3, sym ("define"), getz, mklist (3, sym ("lambda"), q_nil, z)),
      mklist (3, sym ("define"), callwithz,
              mklist (3, sym ("lambda"), mklist (1, z), mklist (1, getz))),
      mklist (2, callwithz, box_int (1)));

  TEST_ASSERT (!safe_eval (l_globalenv, form, &res),
               "expected unbound-error, got no error");
  TEST_ASSERT (eq (f_error_symbol (res), q_error_unbound),
               "expected unbound-error, got %s", errname (res));

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_lexical_counter ()
{
  /*
    set! modifies the binding shared by the closure, each closure has its own:

    (progn
      (define make-counter
        (lambda ()
          (let ((n 0))
            (lambda () (set! n (+ n 1)) n))))
      (define c1 (make-counter))
      (define c2 (make-counter))
      (c1) (c1) (c2)
      (+ (* 10 (c1)) (c2)))
      ---> 32
   */
  Lisp_Object makecounter = make_str_symbol ("make-counter");
  Lisp_Object c1 = make_str_symbol ("c1");
  Lisp_Object c2 = make_str_symbol ("c2");
  Lisp_Object n = make_str_symbol ("n");

  Lisp_Object counter = mklist (
      4, sym ("lambda"), q_nil,
      mklist (3, sym ("set!"), n, mklist (3, sym ("+"), n, box_int (1))), n);
  Lisp_Object let = mklist (3, sym ("let"),
                            mklist (1, mklist (2, n, box_int (0))), counter);

  Lisp_Object form
      = mklist (8, sym ("progn"),
                mklist (3, sym ("define"), makecounter,
                        mklist (3, sym ("lambda"), q_nil, let)),
                mklist (3, sym ("define"), c1, mklist (1, makecounter)),
                mklist (3, sym ("define"), c2, mklist (1, makecounter)),
                mklist (1, c1), mklist (1, c1), mklist (1, c2),
                mklist (3, sym ("+"),
                        mklist (3, sym ("*"), box_int (10), mklist (1, c1)),
                        mklist (1, c2)));

  Lisp_Object res;
  TEST_ASSERT (safe_eval (l_globalenv, form, &res), "unexpected %s",
               errname (res));
  TEST_CHECK_TYPE ("result", res, LISP_INTG);
  TEST_ASSERT (unbox_int (res) == 32, "expected %d, got %ld", 32,
               unbox_int (res));

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_setq_scopes ()
{
  /*
    set! modifies the innermost binding: the global one when there is no local
    binding, the local one when it shadows the global:

    (progn
      (define v 1)
      (set! v 2)
      (define get-v (lambda () v))
      (define set-v (lambda (x) (set! v x)))
      (define shadow (lambda (v) (set! v 10) v))
      (set-v 3)
      (+ (* 100 (shadow 5)) (* 10 (let ((v 7)) (set! v 8) v)) (get-v)))
      ---> 1083
   */
  Lisp_Object v = make_str_symbol ("v");
  Lisp_Object x = make_str_symbol ("x");
  Lisp_Object getv = make_str_symbol ("get-v");
  Lisp_Object setv = make_str_symbol ("set-v");
  Lisp_Object shadow = make_str_symbol ("shadow");

  Lisp_Object let
      = mklist (4, sym ("let"), mklist (1, mklist (2, v, box_int (7))),
                mklist (3, sym ("set!"), v, box_int (8)), v);

  Lisp_Object form = mklist (
      8, sym ("progn"), mklist (3, sym ("define"), v, box_int (1)),
      mklist (3, sym ("set!"), v, box_int (2)),
      mklist (3, sym ("define"), getv, mklist (3, sym ("lambda"), q_nil, v)),
      mklist (3, sym ("define"), setv,
              mklist (3, sym ("lambda"), mklist (1, x),
                      mklist (3, sym ("set!"), v, x))),
      mklist (3, sym ("define"), shadow,
              mklist (4, sym ("lambda"), mklist (1, v),
                      mklist (3, sym ("set!"), v, box_int (10)), v)),
      mklist (2, setv, box_int (3)),
      mklist (4, sym ("+"),
              mklist (3, sym ("*"), box_int (100),
                      mklist (2, shadow, box_int (5))),
              mklist (3, sym ("*"), box_int (10), let), mklist (1, getv)));

  Lisp_Object res;
  TEST_ASSERT (safe_eval (l_globalenv, form, &res), "unexpected %s",
               errname (res));
  TEST_CHECK_TYPE ("result", res, LISP_INTG);
  TEST_ASSERT (unbox_int (res) == 1083, "expected %d, got %ld", 1083,
               unbox_int (res));

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_setq_unbound ()
{
  /*
    set! from an inner scope modifies the binding of the outer scope:

    (let ((n 1))
      (let ((m 2))
        (set! n (+ n m)))
      n)
      ---> 3

    set! of a symbol with no binding is an error, and does not create one,
    both inside a lambda and at top level:

    ((lambda () (set! w 1)))
      ---> unbound-error

    (set! w 1)
      ---> unbound-error

    w
      ---> unbound-error
   */
  Lisp_Object n = make_str_symbol ("n");
  Lisp_Object m = make_str_symbol ("m");
  Lisp_Object w = make_str_symbol ("w");

  Lisp_Object inner = mklist (
      3, sym ("let"), mklist (1, mklist (2, m, box_int (2))),
      mklist (3, sym ("set!"), n, mklist (3, sym ("+"), n, m)));
  Lisp_Object form = mklist (4, sym ("let"),
                             mklist (1, mklist (2, n, box_int (1))), inner, n);

  Lisp_Object res;
  TEST_ASSERT (safe_eval (l_globalenv, form, &res), "unexpected %s",
               errname (res));
  TEST_CHECK_TYPE ("result", res, LISP_INTG);
  TEST_ASSERT (unbox_int (res) == 3, "expected %d, got %ld", 3,
               unbox_int (res));

  Lisp_Object setw = mklist (3, sym ("set!"), w, box_int (1));

  form = mklist (1, mklist (3, sym ("lambda"), q_nil, setw));
  TEST_ASSERT (!safe_eval (l_globalenv, form, &res),
               "expected unbound-error in lambda, got no error");
  TEST_ASSERT (eq (f_error_symbol (res), q_error_unbound),
               "expected unbound-error in lambda, got %s", errname (res));

  TEST_ASSERT (!safe_eval (l_globalenv, setw, &res),
               "expected unbound-error at top level, got no error");
  TEST_ASSERT (eq (f_error_symbol (res), q_error_unbound),
               "expected unbound-error at top level, got %s", errname (res));

  TEST_ASSERT (!safe_eval (l_globalenv, w, &res),
               "expected w to be still unbound");
  TEST_ASSERT (eq (f_error_symbol (res), q_error_unbound),
               "expected unbound-error, got %s", errname (res));

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_lexical_inner_define ()
{
  /*
    Inner defines are visible to closures defined before them in the same
    scope, and do not leak in the global scope:

    (progn
      (define parity
        (lambda (k)
          (define even? (lambda (i) (if (eq? i 0) t (odd? (- i 1)))))
          (define odd? (lambda (i) (if (eq? i 0) nil (even? (- i 1)))))
          (even? k)))
      (parity 10))
      ---> t

    (parity 7)
      ---> nil

    odd?
      ---> unbound-error
   */
  Lisp_Object parity = make_str_symbol ("parity");
  Lisp_Object evenp = make_str_symbol ("even?");
  Lisp_Object oddp = make_str_symbol ("odd?");
  Lisp_Object k = make_str_symbol ("k");
  Lisp_Object i = make_str_symbol ("i");

  Lisp_Object defeven = mklist (
      3, sym ("define"), evenp,
      mklist (
          3, sym ("lambda"), mklist (1, i),
          mklist (4, sym ("if"), mklist (3, sym ("eq?"), i, box_int (0)), q_t,
                  mklist (2, oddp, mklist (3, sym ("-"), i, box_int (1))))));
  Lisp_Object defodd = mklist (
      3, sym ("define"), oddp,
      mklist (
          3, sym ("lambda"), mklist (1, i),
          mklist (4, sym ("if"), mklist (3, sym ("eq?"), i, box_int (0)),
                  q_nil,
                  mklist (2, evenp, mklist (3, sym ("-"), i, box_int (1))))));

  Lisp_Object form
      = mklist (3, sym ("progn"),
                mklist (3, sym ("define"), parity,
                        mklist (5, sym ("lambda"), mklist (1, k), defeven,
                                defodd, mklist (2, evenp, k))),
                mklist (2, parity, box_int (10)));

  Lisp_Object res;
  TEST_ASSERT (safe_eval (l_globalenv, form, &res), "unexpected %s",
               errname (res));
  TEST_ASSERT (eq (res, q_t), "expected (parity 10) to be t");

  form = mklist (2, parity, box_int (7));
  TEST_ASSERT (safe_eval (l_globalenv, form, &res), "unexpected %s",
               errname (res));
  TEST_ASSERT (eq (res, q_nil), "expected (parity 7) to be nil");

  TEST_ASSERT (!safe_eval (l_globalenv, oddp, &res),
               "expected odd? to be unbound in global scope");
  TEST_ASSERT (eq (f_error_symbol (res), q_error_unbound),
               "expected unbound-error, got %s", errname (res));

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_lexical_gc_caller_locals ()
{
  /*
    The locals of the caller survive a gc in the callee. With lexical scope
    the env of the callee does not contain the env of the caller, so it must
    be protected through the stack frames:

    (progn
      (define callee (lambda () (gc) (cons (cons 0 0) (cons 0 0))))
      (define caller
        (lambda (k)
          (let ((l (cons k (cons 2 3))))
            (callee)
            (car (cdr l)))))
      (caller 1))
      ---> 2

    The conses allocated after (gc) try to reuse the blocks of l if they were
    wrongly freed.
   */
  Lisp_Object callee = make_str_symbol ("callee");
  Lisp_Object caller = make_str_symbol ("caller");
  Lisp_Object k = make_str_symbol ("k");
  Lisp_Object l = make_str_symbol ("l");

  Lisp_Object zeros = mklist (3, sym ("cons"), box_int (0), box_int (0));
  Lisp_Object let = mklist (
      4, sym ("let"),
      mklist (1, mklist (2, l,
                         mklist (3, sym ("cons"), k,
                                 mklist (3, sym ("cons"), box_int (2),
                                         box_int (3))))),
      mklist (1, callee), mklist (2, sym ("car"), mklist (2, sym ("cdr"), l)));

  Lisp_Object form = mklist (
      4, sym ("progn"),
      mklist (3, sym ("define"), callee,
              mklist (4, sym ("lambda"), q_nil, mklist (1, sym ("gc")),
                      mklist (3, sym ("cons"), zeros, zeros))),
      mklist (3, sym ("define"), caller,
              mklist (3, sym ("lambda"), mklist (1, k), let)),
      mklist (2, caller, box_int (1)));

  Lisp_Object res;
  TEST_ASSERT (safe_eval (l_globalenv, form, &res), "unexpected %s",
               errname (res));
  TEST_CHECK_TYPE ("result", res, LISP_INTG);
  TEST_ASSERT (unbox_int (res) == 2, "expected %d, got %ld", 2,
               unbox_int (res));

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_eval_lexical_head_eval ()
{
  /*
    The head of a call is evaluated, it can be any expression:

    ((lambda (x) (* x 2)) 21)
      ---> 42

    (progn
      (define make-mul (lambda (n) (lambda (x) (* x n))))
      ((make-mul 3) 7))
      ---> 21
   */
  Lisp_Object makemul = make_str_symbol ("make-mul");
  Lisp_Object n = make_str_symbol ("n");
  Lisp_Object x = make_str_symbol ("x");

  Lisp_Object form = mklist (2,
                             mklist (3, sym ("lambda"), mklist (1, x),
                                     mklist (3, sym ("*"), x, box_int (2))),
                             box_int (21));

  Lisp_Object res;
  TEST_ASSERT (safe_eval (l_globalenv, form, &res), "unexpected %s",
               errname (res));
  TEST_CHECK_TYPE ("result", res, LISP_INTG);
  TEST_ASSERT (unbox_int (res) == 42, "expected %d, got %ld", 42,
               unbox_int (res));

  form = mklist (3, sym ("progn"),
                 mklist (3, sym ("define"), makemul,
                         mklist (3, sym ("lambda"), mklist (1, n),
                                 mklist (3, sym ("lambda"), mklist (1, x),
                                         mklist (3, sym ("*"), x, n)))),
                 mklist (2, mklist (2, makemul, box_int (3)), box_int (7)));

  TEST_ASSERT (safe_eval (l_globalenv, form, &res), "unexpected %s",
               errname (res));
  TEST_CHECK_TYPE ("result", res, LISP_INTG);
  TEST_ASSERT (unbox_int (res) == 21, "expected %d, got %ld", 21,
               unbox_int (res));

  return TEST_RESULT_SUCCESS;
}
