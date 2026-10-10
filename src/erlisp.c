#include "alloc.h"
#include "print.h"
#include "env.h"
#include "eval.h"
#include "lisp.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif /* HAVE_READLINE */

static Lisp_Object
read_eval (Lisp_Object line)
{
  return eval (env_current (), f_read (line));
}

int
main (int argc, char **argv)
{
  init_alloc ();
  init_builtins ();

  if (argc == 1)
    {
      // repl
      printf ("ErLisp v0.1.0\n");

      Lisp_Object res;
      char *line = NULL;
      ssize_t lenline;
      int linum = 1;

      while (1)
        {
#ifdef HAVE_READLINE
          char buf[20];
          sprintf (buf, "erlisp [%3d]> ", linum);
          line = readline (buf);

          if (line == NULL)
            break;

          if (*line)
            add_history (line);

          lenline = strlen (line);
#else /* HAVE_READLINE */
          size_t len = 0;
          printf ("erlisp [%3d]> ", linum);
          fflush (stdout);

          lenline = getline (&line, &len, stdin);
          if (lenline == EOF)
            break;
#endif
          if (lenline == 0)
            continue;

          if (strcmp (line, "exit") == 0)
            {
              free (line);
              break;
            }
          if (condition_case_1 (read_eval, make_string (line), &res))
            {
              print_form (res);
              printf ("\n");
            }
          else
            {
              print_error (res);
            }

          gc ();

          linum++;
        }

      free (line);
      return 0;
    }

  // parse and eval file

  Lisp_Object filename = make_string (argv[1]);
  f_load (filename);

  return 0;
}
