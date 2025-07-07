#include "../src/lisp.h"
#include "test_lib.h"

#include "test_lisp.h"
#include "test_lexer.h"
#include "test_obarray.h"
#include "test_eval.h"
#include "test_env.h"
#include <stdio.h>

int
main (int argc, char **argv)
{
  init_builtins ();

  if (argc > 2)
    {
      fprintf (stderr, "Usage: %s [optional: suite name]", argv[0]);
      return 1;
    }

  char *suitename;

  if (argc == 2)
    suitename = argv[1];
  else
    suitename = NULL; // execute all suites

  TestExecution *te = test_execution_init ();

  // add all suites
  test_execution_add (te, test_suite_lisp ());
  test_execution_add (te, test_suite_lexer ());
  test_execution_add (te, test_suite_obarray ());
  test_execution_add (te, test_suite_eval ());
  test_execution_add (te, test_suite_env ());

  // execute
  int failed = test_execution_run (te, suitename);

  test_execution_cleanup (te);

  return failed;
}
