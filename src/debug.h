#include "lisp.h"

void print_form(Lisp_Object form);
void debug_print_form(Lisp_Object form);
void debug_printf(const char *fmt, ...);
void print_ptr_alignment(void *ptr, size_t align);
