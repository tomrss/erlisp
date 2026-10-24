#include "alloc.h"
#include "env.h"
#include "eval.h"
#include "lisp.h"
#include "print.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif /* HAVE_READLINE */

int
main (int argc, char **argv)
{
  Lisp_Object res;

  init_alloc ();
  init_builtins ();

  if (argc == 1)
    {
      // repl
      printf ("ErLisp v0.1.0\n");

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
          Lisp_Object port = f_open_input_string (make_string (line));

          // build form: "(read port)"
          Lisp_Object readlineform = f_cons (q_read, f_cons (port, q_nil));
          Lisp_Object form;
          if (eval (l_globalenv, readlineform, &form))
            {
              if (eq (form, q_eof))
                continue;

              if (eval_toplevel (form, &res))
                print_form (res, 0);
              else
                print_error (res);
            }
          else
            print_error (form);

          printf ("\n");
          linum++;
        }

      free (line);
      return 0;
    }

  // parse and eval file

  // build form "(load path)"
  Lisp_Object load = f_intern (make_string ("load"));
  Lisp_Object path = make_string (argv[1]);
  Lisp_Object loadform = f_cons (load, f_cons (path, q_nil));

  if (!eval_toplevel (loadform, &res))
    {
      print_error (res);
      return 1;
    }

  return 0;
}
