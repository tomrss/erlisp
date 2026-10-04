#include "alloc.h"
#include "blkalloc.h"
#include "error.h"
#include "lisp.h"
#include "loballoc.h"
#include "print.h"
#include "stack.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SMALL_STRG_NCHRS 16
#define SMALL_VECT_NELTS 16
#define SMALL_LMBD_NARGS 8
#define SMALL_STRG_SIZE                                                       \
  (sizeof (Lisp_String) + SMALL_STRG_NCHRS * sizeof (char))
#define SMALL_VECT_SIZE                                                       \
  (sizeof (Lisp_Vector) + SMALL_VECT_NELTS * sizeof (Lisp_Object))
#define SMALL_LMBD_SIZE                                                       \
  (sizeof (Lisp_Lambda) + SMALL_LMBD_NARGS * sizeof (Lisp_Object))

static inline int
is_small_string (size_t size)
{
  // strict: one char the terminator
  return size < SMALL_STRG_NCHRS;
}

static inline int
is_small_vector (size_t size)
{
  return size <= SMALL_VECT_NELTS;
}

static inline int
is_small_lambda (int maxargs)
{
  return maxargs <= SMALL_LMBD_NARGS;
}

struct heap *heap;

static size_t sum_used_size (struct memstats);

static inline void *
heapblkalloc (blkallocator *blka)
{
  heap->usedsize += blka->blksize;
  return blkalloc (blka);
}

static inline void *
heaploballoc (size_t size)
{
  heap->usedsize += size;
  return loballoc (heap->lobs, size);
}

static void gcmarkobj (Lisp_Object obj);
static void gcmark ();

static struct memstats gcsweep ();

void
init_alloc ()
{
  // fixed-sized types: block size is the size of struct
  heap = malloc (sizeof (struct heap));
  heap->conses = blkalloc_init (sizeof (Lisp_Cons));
  heap->symbols = blkalloc_init (sizeof (Lisp_Symbol));
  // variable-sized types: small objects will be padded in blocks of
  // fixed size. the block size is the size of a flexible array with N
  // elements
  heap->smallstrings = blkalloc_init (SMALL_STRG_SIZE);
  heap->smallvectors = blkalloc_init (SMALL_VECT_SIZE);
  heap->smalllambdas = blkalloc_init (SMALL_LMBD_SIZE);

  // large variable-sized objects will go here
  heap->lobs = loballoc_init ();

  heap->usedsize = 0;
  heap->lastgcused = 0;
  heap->gcgenerations = 0;
}

Lisp_Object
make_cons (Lisp_Object car, Lisp_Object cdr)
{
  Lisp_Cons *cons = heapblkalloc (heap->conses);

  cons->car = car;
  cons->cdr = cdr;

  return box_cons (cons);
}

Lisp_Object
make_vector (size_t size)
{
  Lisp_Vector *vec;
  if (is_small_vector (size))
    {
      // allocate as fixed size holding SMALL_VECT_SIZE elements
      vec = heapblkalloc (heap->smallvectors);
    }
  else
    {
      // allocate in large object heap
      size_t allocsize = sizeof (Lisp_Vector) + size * sizeof (Lisp_Object);
      vec = heaploballoc (allocsize);
    }

  vec->size = size;

  for (size_t i = 0; i < size; ++i)
    vec->contents[i] = q_nil;

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
  Lisp_Object string = make_uninit_string (size);

  memcpy (unbox_string (string)->data, s, size);

  return string;
}

Lisp_Object
make_uninit_string (size_t size)
{
  Lisp_String *string;
  if (is_small_string (size))
    {
      // allocate as fixed sized (with some padding)
      string = heapblkalloc (heap->smallstrings);
    }
  else
    {
      // allocate in large object heap (+1 for terminator)
      size_t allocsize = sizeof (Lisp_String) + (size + 1) * sizeof (char);
      string = heaploballoc (allocsize);
    }

  string->size = size;
  // not counted in size
  string->data[size] = '\0';

  return box_string (string);
}

Lisp_Object
make_symbol (Lisp_Object name)
{
  Lisp_Symbol *symbol = heapblkalloc (heap->symbols);

  symbol->name = name;
  symbol->value = q_unbound;
  symbol->next = NULL;
  symbol->localbound = 0;

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
  // plain allocation, SUBR is not subject to memory management and gc
  Lisp_Subr *subr = malloc (sizeof (Lisp_Subr));

  subr->name = name; // TODO probably safer to copy name // TODO2 never had
                     // problem with this, probably fine this way?
  subr->function = fun;
  subr->minargs = minargs;
  subr->maxargs = maxargs;

  return box_subr (subr);
}

Lisp_Object
make_lambda (int minargs, int maxargs, Lisp_Object env, Lisp_Object *args,
             Lisp_Object form)
{
  Lisp_Lambda *lambda;
  // TODO maybe use a lisp list args instead of c array?
  if (is_small_lambda (maxargs))
    {
      lambda = heapblkalloc (heap->smalllambdas);
    }
  else
    {
      // allocate in large object heap
      size_t allocsize = sizeof (Lisp_Lambda) + maxargs * sizeof (Lisp_Object);
      lambda = heaploballoc (allocsize);
    }

  lambda->minargs = minargs;
  lambda->maxargs = maxargs;
  lambda->env = env;
  for (int i = 0; i < maxargs; i++)
    lambda->args[i] = args[i];
  lambda->form = form;

  return box_lambda (lambda);
}

Lisp_Object
defsubr (const char *name, int minargs, int maxargs, union lisp_subr_fun fun)
{
  if (maxargs != UNEVALLED && maxargs != MANY && maxargs > 8)
    internal_error ("Cannot define subr with %d>8 maxargs, use MANY", maxargs);

  Lisp_Object subr = make_subr (name, minargs, maxargs, fun);
  Lisp_Object symb = make_str_symbol (name);
  unbox_symbol (symb)->value = subr;
  return symb;
}

int
gc_maybe ()
{
  // TODO defines
  const float growthreshold = 5.;
  const size_t minheap = PAGE_SIZE * 32;
  const size_t maxheap = 256 * 1024 * 1024;

  size_t used = heap->usedsize;
  if (used > maxheap)
    {
      gc ();
      return 1;
    }

  if (used > minheap && used > heap->lastgcused * (1 + growthreshold))
    {
      gc ();
      return 1;
    }

  return 0;
}

struct memstats
gc ()
{
  gcmark ();
  struct memstats stats = gcsweep ();
  heap->gcgenerations++;
  // resync the counter with the real usage after sweep
  heap->usedsize = sum_used_size (stats);
  heap->lastgcused = heap->usedsize;
  return stats;
}

static void
gcmarkobj (Lisp_Object obj)
{
  switch (type_of (obj))
    {
    case LISP_STRG:
      if (is_small_string (unbox_string (obj)->size))
        blkgcmark (heap->smallstrings, unbox_string (obj));
      else
        lobgcmark (heap->lobs, unbox_string (obj));
      break;
    case LISP_SYMB:
      if (!blkgcmark (heap->symbols, unbox_symbol (obj)))
        break;
      gcmarkobj (unbox_symbol (obj)->name);
      gcmarkobj (unbox_symbol (obj)->value);
      break;
    case LISP_LMBD:
      if (is_small_lambda (unbox_lambda (obj)->maxargs))
        {
          if (!blkgcmark (heap->smalllambdas, unbox_lambda (obj)))
            break;
        }
      else
        {
          if (!lobgcmark (heap->lobs, unbox_lambda (obj)))
            break;
        }
      gcmarkobj (unbox_lambda (obj)->form);
      gcmarkobj (unbox_lambda (obj)->env);
      for (int i = 0; i < unbox_lambda (obj)->maxargs; i++)
        gcmarkobj (unbox_lambda (obj)->args[i]);
      break;
    case LISP_CONS:
      if (!blkgcmark (heap->conses, unbox_cons (obj)))
        break;
      gcmarkobj (f_car (obj));
      // FIXME in case of a long list, this recursion could be FATAL.
      // replace this with explicit iteration on the cdr
      gcmarkobj (f_cdr (obj));
      break;
    case LISP_VECT:
      if (is_small_vector (unbox_vector (obj)->size))
        {
          if (!blkgcmark (heap->smallvectors, unbox_vector (obj)))
            break;
        }
      else
        {
          if (!lobgcmark (heap->lobs, unbox_vector (obj)))
            break;
        }
      for (size_t i = 0; i < unbox_vector (obj)->size; i++)
        gcmarkobj (unbox_vector (obj)->contents[i]);
      break;
    case LISP_INTG:
    case LISP_SUBR:
      break;
    }
}

static void
gcmarkstackframe (struct stackframe sf)
{
  gcmarkobj (sf.form);
  gcmarkobj (sf.env);
  for (int j = 0; j < sf.nargs; j++)
    gcmarkobj (sf.argvals[j]);
}

static void
gcmark ()
{
  // code that is being evaluated in the stack is a gc root
  stack_walk (gcmarkstackframe);

  // TODO: obarray symbols should be protected from gc in other
  // way. maybe definining a "pure lisp" memory like in Emacs Lisp
  // where predefined objects are allocated and safe from gc
  gcmarkobj (v_obarray);
  Lisp_Vector *obarray = unbox_vector (v_obarray);
  Lisp_Symbol *obs;
  for (size_t i = 0; i < obarray->size; i++)
    {
      obs = unbox_symbol (obarray->contents[i]);
      while (obs)
        {
          gcmarkobj (box_symbol (obs));
          obs = obs->next;
        }
    }
}

static struct memstats
gcsweep ()
{
  // sweep fixed blk memory
  blkgcsweep (heap->conses);
  blkgcsweep (heap->symbols);
  blkgcsweep (heap->smallstrings);
  blkgcsweep (heap->smallvectors);
  blkgcsweep (heap->smalllambdas);

  // sweep large object heap
  lobgcsweep (heap->lobs);

  return memstats ();
}

struct memstats
memstats ()
{
  return (struct memstats){
    .conses = blkstats (heap->conses),
    .symbols = blkstats (heap->symbols),
    .smallstrings = blkstats (heap->smallstrings),
    .smallvectors = blkstats (heap->smallvectors),
    .smalllambdas = blkstats (heap->smalllambdas),
    .loblength = heap->lobs->numblk,
    .lobsize = heap->lobs->size,
  };
}

static size_t
sum_used_size (struct memstats stats)
{
  return stats.conses.sizeused + stats.symbols.sizeused
         + stats.smallstrings.sizeused + stats.smallvectors.sizeused
         + stats.smalllambdas.sizeused + stats.lobsize;
}

static void
print_blkmemstats (const char *name, blkmemstats st)
{
  printf (" %-14s: %3lu pages (%6zu B), %4lu used (%6zu "
          "B)\n",
          name, st.numpages, st.sizepages, st.numused, st.sizeused);
};

void
print_memstats (struct memstats stats)
{
  printf ("Fixed memory blocks:\n");
  size_t freesize = 0;
  printf ("real cons freelist size: %zu\n", freesize);
  print_blkmemstats ("conses", stats.conses);
  print_blkmemstats ("symbols", stats.symbols);
  print_blkmemstats ("small strings", stats.smallstrings);
  print_blkmemstats ("small vectors", stats.smallvectors);
  print_blkmemstats ("small lambda", stats.smalllambdas);
  printf ("Large object heap:\n");
  printf (" %lu objects (%zu B)\n", stats.loblength,
          stats.lobsize);
}

void
memdump ()
{
  printf ("CONS BLOCKS:\n");
  blkmemdump (heap->conses);
  printf ("SYMBOL BLOCKS:\n");
  blkmemdump (heap->symbols);
  printf ("SMALL STRING BLOCKS:\n");
  blkmemdump (heap->smallstrings);
  printf ("SMALL VECTOR BLOCKS:\n");
  blkmemdump (heap->smallvectors);
  printf ("SMALL LAMBDA BLOCKS:\n");
  blkmemdump (heap->smalllambdas);

  printf ("VAR SIZE HEAP:\n");
  lobmemdump (heap->lobs);
}
