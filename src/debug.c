#include "debug.h"
#include "lisp.h"

// TODO very ugly, pls print to string not to stdout

void
print_form (Lisp_Object form)
{
  if (eq (form, q_nil))
    {
      printf ("NIL");
      return;
    }
  if (eq (form, q_t))
    {
      printf ("T");
      return;
    }
  if (eq (form, q_unbound))
    {
      printf ("UNBOUND");
      return;
    }

  switch (type_of (form))
    {
    case LISP_INTG:
      printf ("%ld", (long)unbox_int (form));
      break;
    case LISP_STRG:
      printf ("\"%.*s\"", (int)unbox_string (form)->size,
              unbox_string (form)->data);
      break;
    case LISP_VECT:
      printf ("[size:%zu]", unbox_vector (form)->size);
      break;
    case LISP_SUBR:
      printf ("['%s',%d,%d]", unbox_subr (form)->name,
              unbox_subr (form)->minargs, unbox_subr (form)->maxargs);
      break;
    case LISP_LMBD:
      printf ("[lambda,%d,%d]", unbox_lambda (form)->minargs,
              unbox_lambda (form)->maxargs);
      break;
    case LISP_SYMB:
      printf ("%.*s", (int)unbox_string (unbox_symbol (form)->name)->size,
              unbox_string (unbox_symbol (form)->name)->data);
      break;
    case LISP_CONS:
      printf ("(");
      print_form (unbox_cons (form)->car);
      printf (" . ");
      print_form (unbox_cons (form)->cdr);
      printf (")");
      break;
    }
}

void
print_error (Lisp_Object err)
{
  Lisp_Symbol *sym = unbox_symbol (f_error_symbol (err));
  Lisp_String *data = unbox_string (f_error_data (err));
  fprintf (stderr, "%s: %s\n", unbox_string (sym->name)->data, data->data);

  Lisp_Object backtrace = f_error_backtrace (err);
  Lisp_Object tail = backtrace;

  fprintf (stderr, "Backtrace:\n");
  while (!eq (tail, q_nil))
    {
      const char *fname = unbox_string (f_car (tail))->data;
      fprintf (stderr, " at %s\n", fname);
      tail = f_cdr (tail);
    }
}
