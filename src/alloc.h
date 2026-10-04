#ifndef ALLOC_H
#define ALLOC_H

#include "blkalloc.h"
#include "loballoc.h"
#include "lisp.h"
#include <stddef.h>

#define DEFSUBR(name, minargs, maxargs, fun)                                  \
  defsubr (name, minargs, maxargs, NSUBR (maxargs, fun))

struct heap
{
  blkallocator *conses;
  blkallocator *symbols;
  blkallocator *smallstrings;
  blkallocator *smallvectors;
  blkallocator *smalllambdas;

  loballocator *lobs;

  size_t usedsize;
  size_t lastgcused;
  size_t gcgenerations;
};

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
void print_memstats (struct memstats);
void memdump ();

#endif /* ALLOC_H */
