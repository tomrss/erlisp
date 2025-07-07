#include "alloc.h"
#include "lisp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
  TODO: alignment!!

  We should be allocating with aligned_alloc (MEMALIGN, size); to ensure that
  we do not get pointers that cannot be tagged.  But then what about realloc?
*/

static void memory_full ();

Lisp_Object
make_cons (Lisp_Object car, Lisp_Object cdr)
{
  Lisp_Cons *cons = malloc (sizeof (Lisp_Cons));
  if (!cons)
    memory_full ();

  cons->car = car;
  cons->cdr = cdr;

  return box_cons (cons);
}

Lisp_Object
make_vector (size_t size)
{
  // flexible array allocation
  Lisp_Vector *vec
      = malloc (sizeof (Lisp_Vector) + size * sizeof (Lisp_Object));

  if (!vec)
    memory_full ();

  vec->size = size;

  for (size_t i = 0; i < size; ++i)
    vec->contents[i] = LISP_NULL;

  return box_vector (vec);
}

Lisp_Object
make_string (const char *s)
{
  return make_nstring (s, strlen (s));
}

Lisp_Object
make_nstring (const char *s, size_t size)
{
  Lisp_String *string = malloc (sizeof (Lisp_String));

  char *buf = malloc (size);
  string->data = strncpy (buf, s, size);
  string->size = size;

  return box_string (string);
}

Lisp_Object
make_symbol (Lisp_Object name)
{
  Lisp_Symbol *symbol = malloc (sizeof (Lisp_Symbol));

  symbol->name = name;
  symbol->value = q_unbound;
  symbol->next = NULL;

  return box_symbol (symbol);
}

Lisp_Object
make_str_symbol (const char *name)
{
  return make_symbol (make_string (name));
}

Lisp_Object
make_nstr_symbol (const char *name, size_t size)
{
  return make_symbol (make_nstring (name, size));
}

Lisp_Object
make_subr (const char *name, int minargs, int maxargs, union lisp_subr_fun fun)
{
  Lisp_Subr *subr = malloc (sizeof (Lisp_Subr));

  subr->name = name; // TODO probably safer to copy name
  subr->function = fun;
  subr->minargs = minargs;
  subr->maxargs = maxargs;

  return box_subr (subr);
}

Lisp_Object
make_lambda (int minargs, int maxargs, Lisp_Object *args, Lisp_Object form)
{
  Lisp_Lambda *lambda = malloc (sizeof (Lisp_Lambda));

  lambda->minargs = minargs;
  lambda->maxargs = maxargs;
  lambda->args = args;
  lambda->form = form;

  return box_lambda (lambda);
}

Lisp_Object
defsubr (const char *name, int minargs, int maxargs, union lisp_subr_fun fun)
{
  Lisp_Object subr = make_subr (name, minargs, maxargs, fun);
  Lisp_Object symb = make_str_symbol (name);
  unbox_symbol (symb)->value = subr;
  return symb;
}

void
free_lisp_obj (Lisp_Object o)
{
  if (o == LISP_NULL)
    return;

  if (type_of (o) == LISP_INTG)
    // integer is immediate, not a pointer. nothing to do
    return;

  free (unbox_pointer (0));
}

static void
memory_full ()
{
  // TODO
  fprintf (stderr, "memory full\n");
  exit (1);
}
