#ifndef ALLOC_H
#define ALLOC_H

#include "blkalloc.h" // TODO it is ugly to import here the underlying implementation
#include "lisp.h"
#include <stddef.h>

#define DEFSUBR(name, minargs, maxargs, fun)                                  \
  defsubr (name, minargs, maxargs, NSUBR (maxargs, fun))

struct memstats
{
  // TODO using blkmemstats is handy but depends on underlying impl
  blkmemstats conses;
  blkmemstats symbols;
  blkmemstats smallstrings;
  blkmemstats smallvectors;
  blkmemstats smalllambdas;
  unsigned long int loblength;
  size_t lobsize;
};

Lisp_Object make_string (const char *s);
Lisp_Object make_nstring (const char *s, size_t size);
Lisp_Object make_uninit_string (size_t size);
Lisp_Object make_symbol (Lisp_Object name);
Lisp_Object make_str_symbol (const char *s);
Lisp_Object make_nstr_symbol (const char *s, size_t size);
Lisp_Object make_cons (Lisp_Object car, Lisp_Object cdr);
Lisp_Object make_vector (size_t size);
Lisp_Object make_subr (const char *name, int minargs, int maxargs,
                       union lisp_subr_fun fun);
Lisp_Object make_lambda (int minargs, int maxargs, Lisp_Object env,
                         Lisp_Object *args, Lisp_Object form);
Lisp_Object defsubr (const char *name, int minargs, int maxargs,
                     union lisp_subr_fun fun);
void free_lisp_obj (Lisp_Object o);

void init_alloc ();
struct memstats gc ();
int gc_maybe ();
struct memstats memstats ();
size_t current_used_size ();
size_t last_gcgen_used_size ();
void print_memstats (struct memstats);
void memdump ();

#endif /* ALLOC_H */
