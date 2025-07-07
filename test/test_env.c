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
  unbox_symbol (symb)->value = box_int (123);
  Lisp_Object env;
  env = env_new (l_globalenv, symb);
  env = env_new (env, make_str_symbol ("just"));
  env = env_new (env, make_str_symbol ("to add"));
  env = env_new (env, make_str_symbol ("some vars"));
  Lisp_Object found = env_lookup (env, symb);
  TEST_ASSERT (!eq (found, q_nil), "found is nil");
  TEST_CHECK_TYPE ("found val", unbox_symbol (found)->value, LISP_INTG);
  TEST_ASSERT (unbox_int (unbox_symbol (found)->value) == 123,
               "wrong found val: %ld",
               unbox_int (unbox_symbol (found)->value));
  TEST_ASSERT (eq (env_lookup (l_globalenv, symb), q_nil),
               "lookup in globalenv should go nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_env_notfound ()
{
  Lisp_Object env;
  env = env_new (l_globalenv, q_nil);
  env = env_new (env, make_str_symbol ("just"));
  env = env_new (env, make_str_symbol ("to add"));
  env = env_new (env, make_str_symbol ("some vars"));
  Lisp_Object found = env_lookup (env, make_str_symbol ("notexists!!"));
  TEST_ASSERT (eq (found, q_nil), "found is not nil");
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_env_shadowing ()
{
  int origval = 123;
  int shdwval = 99;
  Lisp_Object orig = make_str_symbol ("test");
  Lisp_Object shdw = make_str_symbol ("test");
  unbox_symbol (orig)->value = box_int (origval);
  unbox_symbol (shdw)->value = box_int (shdwval);
  Lisp_Object parent, child;
  parent = env_new (l_globalenv, orig);
  parent = env_new (parent, make_str_symbol ("just"));
  parent = env_new (parent, make_str_symbol ("to add"));
  child = env_new (parent, make_str_symbol ("some vars"));
  child = env_new (child, shdw);
  child = env_new (child, make_str_symbol ("and some more"));

  Lisp_Object found1 = env_lookup_name (parent, make_string("test"));
  TEST_ASSERT (!eq (found1, q_nil), "original is nil");
  TEST_ASSERT (unbox_int (unbox_symbol (found1)->value) == origval,
               "wrong found val in orig env: %ld",
               unbox_int (unbox_symbol (found1)->value));
  Lisp_Object found2 = env_lookup_name (child, make_string("test"));
  TEST_ASSERT (!eq (found2, q_nil), "shadowed is nil");
  TEST_ASSERT (unbox_int (unbox_symbol (found2)->value) == shdwval,
               "wrong found val in shadowed env: %ld",
               unbox_int (unbox_symbol (found2)->value));
  return TEST_RESULT_SUCCESS;
}
