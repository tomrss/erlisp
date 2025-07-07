#include "eval.h"
#include "lexer.h"
#include "lisp.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

#include "debug.h"

int
main (int argc, char **argv)
{
  Lexer *l;
  Stream *s;
  Lisp_Object prog;
  Lisp_Object res;

  printf ("Tom Lisp v0.1.0\n");

  init_builtins ();
  l = lex_init ();

  if (argc == 1)
    {
      // repl

      char *line = NULL;
      size_t len = 0;
      ssize_t read;
      int line_num = 1;

      while (1)
        {
          printf ("[%3d]> ", line_num);
          fflush (stdout);

          read = getline (&line, &len, stdin);
          if (read == EOF)
            break;

          s = stream_string (line, read);
          lex_set_stream (l, s);
          prog = parse_sexp (l);

          res = eval (l_globalenv, prog);
          print_form (res);
          printf ("\n");

          stream_close (s);
          line_num++;
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
