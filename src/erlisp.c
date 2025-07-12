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
  Lexer *l;
  Stream *s;
  Lisp_Object prog;
  Lisp_Object res;

  printf ("ErLisp v0.1.0\n");

  init_alloc ();
  init_builtins ();
  l = lex_init ();

  if (argc == 1)
    {
      // repl

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
          s = stream_string (line, lenline);
          lex_set_stream (l, s);
          prog = parse_sexp (l);

          res = eval (env_current(), prog);
          print_form (res);
          printf ("\n");
          gc();

          stream_close (s);
          linum++;
        }

      free (line);
      return 0;
    }

  // parse and eval file

  const char *filename = argv[1];
  FILE *f = fopen (filename, "r");
  if (!f)
    {
      fprintf (stderr, "%s: cannot open file\n", filename);
      exit (2);
    }

  s = stream_file (f);
  lex_set_stream (l, s);
  prog = parse (l);
  res = eval (l_globalenv, prog);

  print_form (res);
  printf ("\n");
  fclose (f);
  stream_close (s);
  return 0;
}
