#include "print.h"
#include "lisp.h"

// TODO very ugly, pls print to string not to stdout

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
