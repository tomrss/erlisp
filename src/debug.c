/* #define DEBUG_PRINT 1 */

#include "debug.h"
#include "lisp.h"
#ifdef DEBUG_PRINT
#include <stdarg.h>
#endif /* DEBUG_PRINT */

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
      printf ("%lld", unbox_int (form));
      break;
    case LISP_STRG:
      printf ("\"%s\"", unbox_string (form)->data);
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
      printf ("%s", unbox_string (unbox_symbol (form)->name)->data);
      break;
    case LISP_CONS:
      printf ("(");
      print_form (unbox_cons (form)->car);
      printf (", ");
      print_form (unbox_cons (form)->cdr);
      printf (")");
      break;
    }
}

#ifdef DEBUG_PRINT
void
debug_print_form (Lisp_Object form)
{
  print_form(form);
}
#else  /* DEBUG_PRINT */
void
debug_print_form (Lisp_Object _)
{
}
#endif /* DEBUG_PRINT */

#ifdef DEBUG_PRINT
void
debug_printf (const char *fmt, ...)
{
  va_list vargs;
  va_start (vargs, fmt);
  vprintf (fmt, vargs);
  va_end (vargs);
}
#else  /* DEBUG_PRINT */
void
debug_printf (const char *_, ...)
{
}
#endif /* DEBUG_PRINT */

void print_ptr_alignment(void *ptr, size_t align) {
    printf("Pointer value: %p\n", ptr);
    uintptr_t addr = (uintptr_t)ptr;
    printf("Address as integer: 0x%lx\n", (unsigned long)addr);
    if (addr % align == 0) {
        printf("Pointer is aligned to %zu bytes\n", align);
    } else {
        printf("Pointer is NOT aligned to %zu bytes (offset: %lu)\n", align, addr % align);
    }
}
