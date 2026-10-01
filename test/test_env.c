#include "../src/alloc.h"
#include "../src/env.h"
#include "../src/lisp.h"
#include "../src/obarray.h"
#include "test_lib.h"

// test cases
static TestResult test_env_symbols ();
static TestResult test_env_obarray ();
static TestResult test_env_found ();
static TestResult test_env_notfound ();
static TestResult test_env_shadowing ();

static TestCase test_env_cases[] = {
  { .skip = 0, .name = "symbols", .run = &test_env_symbols },
  { .skip = 0, .name = "obarray", .run = &test_env_obarray },
  { .skip = 0, .name = "found", .run = &test_env_found },
  { .skip = 0, .name = "nofound", .run = &test_env_notfound },
  { .skip = 0, .name = "shadowing", .run = &test_env_shadowing },
  {}, // terminator
};

TestSuite *
test_suite_env ()
{
  return test_suite_init ("env", test_env_cases);
}

// test cases implementation

static TestResult
test_env_symbols ()
{
  TEST_CHECK_TYPE ("q_nil", q_nil, LISP_SYMB);
  TEST_CHECK_TYPE ("q_t", q_nil, LISP_SYMB);
  TEST_CHECK_TYPE ("q_unbound", q_nil, LISP_SYMB);
  TEST_CHECK_TYPE ("l_globalenv", v_obarray, LISP_VECT);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_env_obarray ()
{
  TEST_CHECK_TYPE ("v_obarray", v_obarray, LISP_VECT);

  Lisp_Object fsymb;

  fsymb = obarray_lookup_name (v_obarray, make_string ("cons"));
  TEST_CHECK_TYPE ("cons symb", fsymb, LISP_SYMB);
  TEST_CHECK_TYPE ("cons subr", unbox_symbol (fsymb)->value, LISP_SUBR);
  TEST_ASSERT (unbox_subr (unbox_symbol (fsymb)->value)->maxargs == 2,
               "cons maxargs");
  fsymb = obarray_lookup_name (v_obarray, make_string ("eval"));
  TEST_CHECK_TYPE ("eval symb", fsymb, LISP_SYMB);
  TEST_CHECK_TYPE ("eval subr", unbox_symbol (fsymb)->value, LISP_SUBR);
  TEST_ASSERT (unbox_subr (unbox_symbol (fsymb)->value)->maxargs == 1,
               "eval maxargs");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_env_found ()
{
  Lisp_Object name = make_string ("test");
  Lisp_Object symb = make_symbol (name);
  Lisp_Object env;
  env = env_new (l_globalenv);
  env_define (env, symb, box_int (123));
  env_define (env, make_str_symbol ("just"), q_nil);
  env_define (env, make_str_symbol ("to add"), q_nil);
  env_define (env, make_str_symbol ("some vars"), q_nil);
  Lisp_Object found = env_lookup (env, symb);
  TEST_ASSERT (!eq (found, q_nil), "found is nil");
  TEST_CHECK_TYPE ("found val", found, LISP_INTG);
  TEST_ASSERT (unbox_int (found) == 123, "wrong found val: %ld",
               unbox_int (found));
  TEST_ASSERT (eq (env_lookup (l_globalenv, symb), q_unbound),
               "lookup in globalenv should go unbound");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_env_notfound ()
{
  Lisp_Object env;
  env = env_new (l_globalenv);
  env_define (env, make_str_symbol ("only"), q_nil);
  env_define (env, make_str_symbol ("just"), q_nil);
  env_define (env, make_str_symbol ("to add"), q_nil);
  env_define (env, make_str_symbol ("some vars"), q_nil);
  Lisp_Object found = env_lookup (env, make_str_symbol ("notexists!!"));
  TEST_ASSERT (eq (found, q_unbound), "found is not unbound");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_env_shadowing ()
{
  int origval = 123;
  int shdwval = 99;
  // lookup is by identity: shadowing binds the same symbol again
  Lisp_Object symb = make_str_symbol ("test");
  Lisp_Object parent, child;
  parent = env_new (l_globalenv);
  env_define (parent, symb, box_int (origval));
  env_define (parent, make_str_symbol ("just"), q_nil);
  env_define (parent, make_str_symbol ("to add"), q_nil);
  child = env_new (parent);
  env_define (child, make_str_symbol ("some vars"), q_nil);
  env_define (child, symb, box_int (shdwval));
  env_define (child, make_str_symbol ("and some more"), q_nil);

  Lisp_Object found1 = env_lookup (parent, symb);
  TEST_ASSERT (!eq (found1, q_nil), "original is nil");
  TEST_ASSERT (unbox_int (found1) == origval,
               "wrong found val in orig env: %ld", unbox_int (found1));
  Lisp_Object found2 = env_lookup (child, symb);
  TEST_ASSERT (!eq (found2, q_nil), "shadowed is nil");
  TEST_ASSERT (unbox_int (found2) == shdwval,
               "wrong found val in shadowed env: %ld", unbox_int (found2));
  return TEST_RESULT_SUCCESS;
}
