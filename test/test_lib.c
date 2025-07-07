#include "test_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TestSuite *
test_suite_init (char *name, TestCase *cases)
{
  TestSuite *suite;

  suite = malloc (sizeof (TestSuite));

  suite->name = name;
  suite->cases = (TestCase *)cases;
  suite->executed = 0;
  suite->failed = 0;
  suite->skipped = 0;

  // compute size cycling until terminator 
  int i = 0;
  TestCase tc;
  while ((tc = cases[i]).name != NULL && tc.run != NULL)
    i++;
  suite->size = i;

  return suite;
}

int
test_suite_run (TestSuite *suite)
{
  for (int i = 0; i < suite->size; i++)
    {
      TestCase tc;
      TestResult res;

      tc = suite->cases[i];

      printf ("test %10s - %-15s... ", suite->name, tc.name);

      if (tc.skip)
        {
          printf (ANSI_COLOR_YELLOW "SKIPPED\n" ANSI_COLOR_RESET);
          suite->skipped++;
        }
      else
        {
          res = tc.run ();

          if (res.success)
            {
              printf (ANSI_COLOR_GREEN "SUCCESS\n" ANSI_COLOR_RESET);
            }
          else
            {
              printf (ANSI_COLOR_RED "FAIL" ANSI_COLOR_RESET "    %s\n",
                      res.reason);
              suite->failed++;
            }

          suite->executed++;
        }
    }

  return 0;
}

TestExecution *
test_execution_init ()
{
  TestExecution *te;
  int bufsize;
  int alloc;

  bufsize = 20;
  alloc = bufsize;
  te = malloc (sizeof (TestExecution));
  te->suites = malloc (alloc * sizeof (TestSuite));
  te->alloc = alloc;
  te->size = 0;

  return te;
}

int
test_execution_add (TestExecution *te, TestSuite *suite)
{
  if (te->size >= te->alloc)
    {
      te->alloc += te->bufsize;
      te->suites = realloc (te->suites, te->alloc);
      if (!te->suites)
        {
          fprintf (stderr, "Out of memory reallocating test run suites\n");
          return 1;
        }
    }

  te->suites[te->size] = suite;
  te->size++;

  return 0;
}

int
test_execution_run (TestExecution *te, char *suitename)
{
  int executed = 0;
  int failed = 0;
  int skipped = 0;

  printf ("\n");

  for (int i = 0; i < te->size; i++)
    {
      TestSuite *suite = te->suites[i];

      if (suitename != NULL && !strcmp (suitename, suite->name))
        {
          continue;
        }

      test_suite_run (suite);

      executed += suite->executed;
      failed += suite->failed;
      skipped += suite->skipped;
    }

  char *color;
  if (failed)
    {
      color = ANSI_COLOR_RED;
    }
  else
    {
      color = ANSI_COLOR_GREEN;
    }

  printf ("\nTest execution completed: " ANSI_COLOR_BLUE "%d" ANSI_COLOR_RESET
          " executed, %s%d%s failed, " ANSI_COLOR_YELLOW "%d" ANSI_COLOR_RESET
          " skipped\n\n",
          executed, color, failed, ANSI_COLOR_RESET, skipped);

  return failed;
}

void
test_execution_cleanup (TestExecution *te)
{
  for (int i = 0; i < te->size; i++)
    {
      free (te->suites[i]);
    }
  free (te->suites);
  free (te);
}
