#include "lisp.h"
#include <stdio.h>

void port_printf (Lisp_Port *p, const char *fmt, ...);
void print_form (Lisp_Object form, int display);
void fprint_form (Lisp_Port *stream, Lisp_Object form, int display);
void print_error (Lisp_Object err);
void print_ptr_alignment (void *ptr, size_t align);
