#include "print.h"
#include "lisp.h"
#include <math.h>

// TODO very ugly, pls print to string not to stdout

static void
fprint_float (FILE *stream, Lisp_Float f)
{
  if (isnan (f))
    {
      fprintf (stream, "+nan.0");
      return;
    }
  if (isinf (f))
    {
      fprintf (stream, f > 0 ? "+inf.0" : "-inf.0");
      return;
    }

  // shortest representation that reads back to the same value: 17
  // significant digits are always enough for a double
  char buf[32];
  for (int prec = 1; prec <= 17; prec++)
    {
      snprintf (buf, sizeof buf, "%.*g", prec, (double)f);
      if ((Lisp_Float)strtod (buf, NULL) == f)
        break;
    }

  // always look like a float, so that 1.0 is not printed as 1
  if (!strpbrk (buf, ".e"))
    strcat (buf, ".0");

  fprintf (stream, "%s", buf);
}

// mirror of parse_char in lexer.c, so that a printed char reads back
static void
fprint_char (FILE *stream, Lisp_Char c)
{
  switch (c)
    {
    case '\n':
      fprintf (stream, "?\\n");
      break;
    case '\t':
      fprintf (stream, "?\\t");
      break;
    case '\r':
      fprintf (stream, "?\\r");
      break;
    case ' ':
      fprintf (stream, "?\\s");
      break;
    case 27:
      fprintf (stream, "?\\e");
      break;
    case '\0':
      fprintf (stream, "?\\0");
      break;
    case '\\':
      fprintf (stream, "?\\\\");
      break;
    default:
      fprintf (stream, "?%c", c);
    }
}

void
print_form (Lisp_Object form)
{
  fprint_form (stdout, form);
}

void
fprint_form (FILE *stream, Lisp_Object form)
{
  if (nil (form))
    {
      fprintf (stream, "NIL");
      return;
    }
  if (eq (form, q_t))
    {
      fprintf (stream, "T");
      return;
    }
  if (unbound (form))
    {
      fprintf (stream, "UNBOUND");
      return;
    }

  switch (type_of (form))
    {
    case LISP_INTG:
      fprintf (stream, "%ld", (long)unbox_int (form));
      break;
    case LISP_STRG:
      fprintf (stream, "\"%.*s\"", (int)unbox_string (form)->size,
               unbox_string (form)->data);
      break;
    case LISP_VECT:
      fprintf (stream, "[size:%zu]", unbox_vector (form)->size);
      break;
    case LISP_SUBR:
      fprintf (stream, "['%s',%d,%d]", unbox_subr (form)->name,
               unbox_subr (form)->minargs, unbox_subr (form)->maxargs);
      break;
    case LISP_LMBD:
      fprintf (stream, "[lambda,%d,%d]", unbox_lambda (form)->minargs,
               unbox_lambda (form)->maxargs);
      break;
    case LISP_SYMB:
      fprintf (stream, "%.*s",
               (int)unbox_string (unbox_symbol (form)->name)->size,
               unbox_string (unbox_symbol (form)->name)->data);
      break;
    case LISP_CONS:
      fprintf (stream, "(");
      fprint_form (stream, unbox_cons (form)->car);
      fprintf (stream, " . ");
      fprint_form (stream, unbox_cons (form)->cdr);
      fprintf (stream, ")");
      break;
    case LISP_FLOT:
      fprint_float (stream, unbox_float (form));
      break;
    case LISP_CHAR:
      fprint_char (stream, unbox_char (form));
      break;
    }
}

void
print_error (Lisp_Object err)
{
  fprint_form (stderr, f_error_symbol (err));
  fprintf (stderr, ":");

  // data is usually a list of objects, e.g. (symbol) for unbound-error:
  // print its elements. anything else (e.g. an int) is printed as is
  Lisp_Object data = f_error_data (err);
  if (type_of (data) == LISP_CONS)
    for (Lisp_Object tail = data; type_of (tail) == LISP_CONS;
         tail = unbox_cons (tail)->cdr)
      {
        fprintf (stderr, " ");
        fprint_form (stderr, unbox_cons (tail)->car);
      }
  else if (!nil (data))
    {
      fprintf (stderr, " ");
      fprint_form (stderr, data);
    }
  fprintf (stderr, "\n");

  Lisp_Object backtrace = f_error_backtrace (err);
  Lisp_Object tail = backtrace;

  fprintf (stderr, "Backtrace:\n");
  while (!nil (tail))
    {
      const char *fname = unbox_string (f_car (tail))->data;
      fprintf (stderr, " at %s\n", fname);
      tail = f_cdr (tail);
    }
}
