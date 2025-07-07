#ifndef _TEST_LIB_H_
#define _TEST_LIB_H_

#include "stdarg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define TEST_RESULT_SUCCESS (TestResult){ .success = 1, .reason = "OK" }
#define TEST_RESULT_FAIL(...)                                                 \
  (TestResult) { .success = 0, .reason = test_real_sprintf (__VA_ARGS__) }

#define TEST_CHECK_TYPE(objname, obj, type)                                   \
  if (type_of (obj) != type)                                                  \
    return TEST_RESULT_FAIL ("%s: expected type %s, got %s", objname,         \
                             type_name (type), type_name (type_of (obj)));

#define TEST_ASSERT(expr, fmt, ...)                                           \
  do {                                                                        \
    if (!(expr))                                                              \
      return TEST_RESULT_FAIL(fmt, ##__VA_ARGS__);                            \
  } while (0)

// a sprintf that is confortable to use but not so efficient
static inline char *
test_real_sprintf (const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  int size = vsnprintf (NULL, 0, fmt, args);
  va_end (args);

  char *buf = malloc (size + 1);
  if (!buf)
    return NULL;

  va_start (args, fmt);
  vsnprintf (buf, size + 1, fmt, args);
  va_end (args);

  return buf;
}

typedef struct TestResult TestResult;
typedef struct TestCase TestCase;
typedef struct TestSuite TestSuite;
typedef struct TestExecution TestExecution;

struct TestResult
{
  int success;
  char *reason;
};

struct TestCase
{
  char *name;
  TestResult (*run) ();
  int skip;
};

struct TestSuite
{
  char *name;
  TestCase *cases;
  int size;
  int executed;
  int failed;
  int skipped;
};

struct TestExecution
{
  TestSuite **suites;
  int size;
  int alloc;
  int bufsize;
};

TestSuite *test_suite_init (char *name, TestCase *cases);

int test_suite_run (TestSuite *suite);

TestExecution *test_execution_init ();

int test_execution_add (TestExecution *te, TestSuite *suite);

int test_execution_run (TestExecution *te, char *suitename);

void test_execution_cleanup (TestExecution *te);

#endif /* _TEST_LIB_H_ */
