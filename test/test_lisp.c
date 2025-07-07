#include "test_lisp.h"
#include "../src/alloc.h"
#include "../src/lisp.h"
#include "test_lib.h"
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>

// test cases
static TestResult test_lisp_int32 ();
static TestResult test_lisp_negint32 ();
static TestResult test_lisp_bigint32 ();
static TestResult test_lisp_int64 ();
static TestResult test_lisp_negint64 ();
static TestResult test_lisp_bigint64 ();
static TestResult test_lisp_string ();
static TestResult test_lisp_nstring ();
static TestResult test_lisp_symbol ();
static TestResult test_lisp_cons ();
static TestResult test_lisp_cons_nested ();
static TestResult test_lisp_vector ();

static TestCase test_lisp_cases[] = {
  { .skip = 0, .name = "int32", .run = &test_lisp_int32 },
  { .skip = 0, .name = "negint32", .run = &test_lisp_negint32 },
  { .skip = 0, .name = "bigint32", .run = &test_lisp_bigint32 },
  { .skip = 0, .name = "int64", .run = &test_lisp_int64 },
  { .skip = 0, .name = "negint64", .run = &test_lisp_negint64 },
  { .skip = 0, .name = "bigint64", .run = &test_lisp_bigint64 },
  { .skip = 0, .name = "string", .run = &test_lisp_string },
  { .skip = 0, .name = "nstring", .run = &test_lisp_nstring },
  { .skip = 0, .name = "symbol", .run = &test_lisp_symbol },
  { .skip = 0, .name = "cons", .run = &test_lisp_cons },
  { .skip = 0, .name = "cons_nested", .run = &test_lisp_cons_nested },
  { .skip = 0, .name = "vector", .run = &test_lisp_vector },
  {}, // terminator
};

TestSuite *
test_suite_lisp ()
{
  return test_suite_init ("lisp", test_lisp_cases);
}

// test cases implementation

static TestResult
test_lisp_int32 ()
{
  int i = 1234;
  Lisp_Object li = box_int (i);

  TEST_CHECK_TYPE ("int", li, LISP_INTG);

  Lisp_Integer ui = unbox_int (li);

  if (ui != i)
    return TEST_RESULT_FAIL ("int unboxing produced %d, expected %d", ui, i);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lisp_negint32 ()
{
  int i = -1234;
  Lisp_Object li = box_int (i);

  TEST_CHECK_TYPE ("int", li, LISP_INTG);

  Lisp_Integer ui = unbox_int (li);

  if (ui != i)
    return TEST_RESULT_FAIL ("int unboxing produced %d, expected %d", ui, i);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lisp_bigint32 ()
{
  int i = INT_MAX;
  Lisp_Object li = box_int (i);

  TEST_CHECK_TYPE ("int", li, LISP_INTG);

  Lisp_Integer ui = unbox_int (li);

  if (ui != i)
    return TEST_RESULT_FAIL ("int unboxing produced %d, expected %d", ui, i);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lisp_int64 ()
{
  int64_t i = 1234;
  Lisp_Object li = box_int (i);

  TEST_CHECK_TYPE ("int", li, LISP_INTG);

  Lisp_Integer ui = unbox_int (li);

  if (ui != i)
    return TEST_RESULT_FAIL ("int unboxing produced %ld, expected %ld", ui, i);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lisp_negint64 ()
{
  int64_t i = -1234;
  Lisp_Object li = box_int (i);

  TEST_CHECK_TYPE ("int", li, LISP_INTG);

  Lisp_Integer ui = unbox_int (li);

  if (ui != i)
    return TEST_RESULT_FAIL ("int unboxing produced %ld, expected %ld", ui, i);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lisp_bigint64 ()
{
  // maximum integer due to tagging with 3 bits
  int64_t i = INT64_MAX >> 3;
  Lisp_Object li = box_int (i);

  TEST_CHECK_TYPE ("int", li, LISP_INTG);

  Lisp_Integer ui = unbox_int (li);

  if (ui != i)
    return TEST_RESULT_FAIL ("int unboxing produced %ld, expected %ld", ui, i);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lisp_string ()
{
  char *str = "some testing string!!";
  size_t strlen = 21; // len of str above

  Lisp_Object lstr = make_string (str);

  TEST_CHECK_TYPE ("str", lstr, LISP_STRG);

  Lisp_String *ustr = unbox_string (lstr);

  if (ustr->size != strlen)
    return TEST_RESULT_FAIL ("str len: expected %d, got %d", strlen,
                             ustr->size);

  if (strncmp (ustr->data, str, ustr->size))
    return TEST_RESULT_FAIL ("str data: expected %s, got %s", str, ustr->data);

  free_lisp_obj (lstr);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lisp_nstring ()
{
  char *fullstr = "some testing string!!######EXTRA CHARS IGNOREDDDDD";
  char *str = "some testing string!!";
  size_t strlen = 21; // len of str above

  Lisp_Object lstr = make_nstring (fullstr, 21);

  TEST_CHECK_TYPE ("str", lstr, LISP_STRG);

  Lisp_String *ustr = unbox_string (lstr);

  if (ustr->size != strlen)
    return TEST_RESULT_FAIL ("str len: expected %d, got %d", strlen,
                             ustr->size);

  if (strncmp (ustr->data, str, ustr->size))
    return TEST_RESULT_FAIL ("str data: expected %s, got %s", str, ustr->data);

  fullstr[0] = 'X';
  if (ustr->data[0] != 's')
    return TEST_RESULT_FAIL (
        "str was not copied!! modified by original pointer");

  free_lisp_obj (lstr);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lisp_symbol ()
{
  char *name = "mysymbol";
  size_t namelen = 8;
  Lisp_Object lname = make_string (name);
  Lisp_Object lsymbol = make_symbol (lname);

  TEST_CHECK_TYPE ("symbol", lsymbol, LISP_SYMB);

  Lisp_Symbol *usymbol = unbox_symbol (lsymbol);
  Lisp_String *uname = unbox_string (usymbol->name);

  if (uname->size != namelen)
    return TEST_RESULT_FAIL ("name len: expected %d, got %d", namelen,
                             uname->size);

  if (strncmp (uname->data, name, uname->size))
    return TEST_RESULT_FAIL ("name data: expected %s, got %s", name,
                             uname->data);

  free_lisp_obj (lsymbol);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lisp_cons ()
{
  int car = 1;
  int cdr = -12;

  Lisp_Object lcar = box_int (car);
  Lisp_Object lcdr = box_int (cdr);

  Lisp_Object lcons = make_cons (lcar, lcdr);

  TEST_CHECK_TYPE ("cons", lcons, LISP_CONS);

  Lisp_Cons *ucons = unbox_cons (lcons);

  if (ucons->car != lcar)
    return TEST_RESULT_FAIL ("car reference different");
  if (ucons->cdr != lcdr)
    return TEST_RESULT_FAIL ("cdr reference different");
  if (unbox_int (ucons->car) != car)
    return TEST_RESULT_FAIL ("car unboxed: expected %d, got %d",
                             unbox_int (ucons->car), car);
  if (unbox_int (ucons->cdr) != cdr)
    return TEST_RESULT_FAIL ("cdr unboxed: expected %d, got %d",
                             unbox_int (ucons->cdr), cdr);

  free_lisp_obj (lcar);
  free_lisp_obj (lcdr);
  free_lisp_obj (lcons);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lisp_cons_nested ()
{
  // like this: (1 . (-12 . (999 . "bleb")))
  int cari = 1;
  int cdrcar = -12;
  int cdrcdrcar = 999;
  char *cdrcdrcdr = "bleb";

  Lisp_Object lcar = box_int (cari);
  Lisp_Object lcdrcar = box_int (cdrcar);
  Lisp_Object lcdrcdrcar = box_int (cdrcdrcar);
  Lisp_Object lcdrcdrcdr = make_string (cdrcdrcdr);

  Lisp_Object lconscdrcdr = make_cons (lcdrcdrcar, lcdrcdrcdr);
  Lisp_Object lconscdr = make_cons (lcdrcar, lconscdrcdr);
  Lisp_Object lcons = make_cons (lcar, lconscdr);

  TEST_CHECK_TYPE ("cons", lcons, LISP_CONS);

  if (f_car (lcons) != lcar)
    return TEST_RESULT_FAIL ("car reference different");
  if (f_cdr (lcons) != lconscdr)
    return TEST_RESULT_FAIL ("cdr reference different");
  if (f_car (f_cdr (lcons)) != lcdrcar)
    return TEST_RESULT_FAIL ("cdr car reference different");
  if (f_cdr (f_cdr (lcons)) != lconscdrcdr)
    return TEST_RESULT_FAIL ("cdr cdr reference different");

  if (type_of (f_car (lcons)) != LISP_INTG)
    return TEST_RESULT_FAIL ("cdr wrong type");
  if (unbox_int (f_car (lcons)) != cari)
    return TEST_RESULT_FAIL ("car unboxed: expected %d, got %d",
                             unbox_int (f_car (lcons)), cari);
  if (type_of (f_cdr (f_cdr (lcons))) != LISP_CONS)
    return TEST_RESULT_FAIL ("cdr cdr wrong type");
  Lisp_Object leafcdr = f_cdr (f_cdr (f_cdr (lcons)));
  if (type_of (leafcdr) != LISP_STRG)
    return TEST_RESULT_FAIL ("cdr cdr cdr wrong type");
  Lisp_String *ustr = unbox_string (leafcdr);
  if (ustr->size != 4 || strncmp (ustr->data, cdrcdrcdr, 4))
    return TEST_RESULT_FAIL ("cdr cdr cdr unboxed: expected '%s', got '%s'",
                             cdrcdrcdr, ustr->data);

  free_lisp_obj (lcons);
  free_lisp_obj (lconscdr);
  free_lisp_obj (lconscdrcdr);

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_lisp_vector ()
{
  size_t len = 4;
  Lisp_Object lvec = make_vector (len);

  TEST_CHECK_TYPE ("vec", lvec, LISP_VECT);

  Lisp_Vector *uvec = unbox_vector (lvec);
  if (uvec->size != len)
    return TEST_RESULT_FAIL ("vec size: expected %ld, got %ld", len,
                             uvec->size);

  for (size_t i = 0; i < len; i++)
    {
      if (uvec->contents[i] != LISP_NULL)
        return TEST_RESULT_FAIL ("vec not initialized correctly");
    }

  free_lisp_obj (lvec);

  return TEST_RESULT_SUCCESS;
}
