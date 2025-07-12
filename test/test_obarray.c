#include "../src/alloc.h"
#include "../src/obarray.h"
#include "test_lib.h"

// test cases
static TestResult test_obarray_init ();
static TestResult test_obarray_empty ();
static TestResult test_obarray_notfound ();
static TestResult test_obarray_found ();
static TestResult test_obarray_foundall ();

static TestCase test_obarray_cases[] = {
  { .skip = 0, .name = "init", .run = &test_obarray_init },
  { .skip = 0, .name = "empty", .run = &test_obarray_empty },
  { .skip = 0, .name = "notfound", .run = &test_obarray_notfound },
  { .skip = 0, .name = "found", .run = &test_obarray_found },
  { .skip = 0, .name = "foundall", .run = &test_obarray_foundall },
  {}, // terminator
};

TestSuite *
test_suite_obarray ()
{
  return test_suite_init ("obarray", test_obarray_cases);
}

// test cases implementation

static TestResult
test_obarray_init ()
{
  Lisp_Object obarray = obarray_init ();

  TEST_CHECK_TYPE ("obarray", obarray, LISP_VECT);

  Lisp_Vector *uobarray = unbox_vector (obarray);

  if (uobarray->size != OBARRAY_BUCKETS)
    return TEST_RESULT_FAIL ("obarray size: %d different from buckets num %d",
                             uobarray->size, OBARRAY_BUCKETS);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_obarray_empty ()
{
  Lisp_Object obarray = obarray_init ();

  TEST_CHECK_TYPE ("obarray", obarray, LISP_VECT);

  Lisp_Object symbol = obarray_lookup_name (obarray, make_string ("notexist"));

  // obarray_lookup_name returns INT representing the wannabe hash if not found
  TEST_CHECK_TYPE ("symbol found", symbol, LISP_INTG);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_obarray_notfound ()
{
  Lisp_Object obarray = obarray_init ();
  TEST_CHECK_TYPE ("obarray", obarray, LISP_VECT);

  int n = 1000;

  Lisp_Object symbols[n];

  for (int i = 0; i < n; i++)
    {
      char buf[5];
      snprintf (buf, sizeof (buf), "s%d", i);
      Lisp_Object s = make_symbol (make_string (buf));
      Lisp_Object a = obarray_put (obarray, s);
      TEST_CHECK_TYPE ("symbol added", a, LISP_SYMB);
      Lisp_Symbol *us = unbox_symbol (s);
      Lisp_Symbol *ua = unbox_symbol (a);
      if (unbox_string (us->name)->size != unbox_string (ua->name)->size
          || strncmp (unbox_string (us->name)->data,
                      unbox_string (ua->name)->data,
                      unbox_string (ua->name)->size))
        return TEST_RESULT_FAIL ("i %d: symbol in: '%s', symbol added: '%s'",
                                 i, unbox_string (us->name)->data,
                                 unbox_string (ua->name)->data);
      if (a != s)
        return TEST_RESULT_FAIL ("added different from input symbol");
      symbols[i] = s;
    }

  for (int i = 0; i < n; i++)
    {
      char buf[20];
      snprintf (buf, sizeof (buf), "NOTEXIST%d", i);
      Lisp_Object found = obarray_lookup_name (obarray, make_string (buf));
      TEST_CHECK_TYPE ("symbol not found", found, LISP_INTG);
    }

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_obarray_found ()
{
  Lisp_Object obarray = obarray_init ();
  TEST_CHECK_TYPE ("obarray", obarray, LISP_VECT);

  Lisp_Object s = make_symbol (make_string ("s"));
  Lisp_Object added = obarray_put (obarray, s);
  if (added != s)
    return TEST_RESULT_FAIL ("added different from input symbol");

  Lisp_Object found = obarray_lookup_name (obarray, make_string ("s"));
  TEST_CHECK_TYPE ("symbol found", found, LISP_SYMB);
  if (s != found)
    return TEST_RESULT_FAIL ("symbol expected: '%s', found '%s'",
                             unbox_string (unbox_symbol (s)->name)->data,
                             unbox_string (unbox_symbol (found)->name)->data);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_obarray_foundall ()
{
  Lisp_Object obarray = obarray_init ();
  TEST_CHECK_TYPE ("obarray", obarray, LISP_VECT);

  int n = 1000;

  Lisp_Object symbols[n];

  for (int i = 0; i < n; i++)
    {
      char buf[5];
      snprintf (buf, sizeof (buf), "s%d", i);
      Lisp_Object s = make_symbol (make_string (buf));
      Lisp_Object added = obarray_put (obarray, s);
      if (added != s)
        return TEST_RESULT_FAIL ("added different from input symbol");
      symbols[i] = s;
    }

  for (int i = 0; i < n; i++)
    {
      char buf[5];
      snprintf (buf, sizeof (buf), "s%d", i);
      Lisp_Object found = obarray_lookup_name (obarray, make_string (buf));
      TEST_CHECK_TYPE ("symbol found", found, LISP_SYMB);
      if (symbols[i] != found)
        return TEST_RESULT_FAIL (
            "symbol expected: '%s', found '%s'",
            unbox_string (unbox_symbol (symbols[i])->name)->data,
            unbox_string (unbox_symbol (found)->name)->data);
    }

  return TEST_RESULT_SUCCESS;
}
