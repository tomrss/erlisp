#include "lisp.h"

#define LPRINT(msg, lisp)                                                     \
  printf (msg);                                                               \
  print_form (lisp);                                                          \
  printf ("\n");

void print_form (Lisp_Object form);
void print_error (Lisp_Object err);
void print_ptr_alignment (void *ptr, size_t align);

/* #define DEBUG_PRINT 1 */

// macros and not functions: when debug print is disabled the calls must
// disappear completely, even an empty function costs a call in eval
#ifdef DEBUG_PRINT
#define debug_print_form(form) print_form (form)
#define debug_printf(...) printf (__VA_ARGS__)
#else /* DEBUG_PRINT */
#define debug_print_form(form) ((void)(form))
#define debug_printf(...) ((void)0)
#endif /* DEBUG_PRINT */
