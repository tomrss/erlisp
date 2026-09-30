#include "alloc.h"
#include "debug.h"
#include "env.h"
#include "eval.h"
#include "lexer.h"
#include "lisp.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif /* HAVE_READLINE */

int
main (int argc, char **argv)
{
  init_alloc ();
  init_builtins ();

  if (argc == 1)
    {
      // repl
      printf ("ErLisp v0.1.0\n");

      Lexer *l;
      Lisp_Object prog;
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
          l = lex_init (stream_string (line, lenline));

          prog = parse_sexp (l);
          if (safe_eval (env_current (), prog, &res))
            {
              print_form (res);
              printf ("\n");
            }
          else
            {
              print_error (res);
            }

          gc ();

          lex_close (l);
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
