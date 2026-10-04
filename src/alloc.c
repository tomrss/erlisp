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

blkallocator *all_cons;
blkallocator *all_symbol;
blkallocator *all_smallstring;
blkallocator *all_smallvector;
blkallocator *all_smalllambda;

struct lobheap *lobheap;

static struct memstats lastgcstats;
static unsigned long gcgen;

static size_t sum_used_size (struct memstats);

static void gcmarkobj (Lisp_Object obj);
static void gcmark ();
static struct memstats gcsweep ();

void
init_alloc ()
{
  // fixed-sized types: block size is the size of struct
  all_cons = blkalloc_init (sizeof (Lisp_Cons));
  all_symbol = blkalloc_init (sizeof (Lisp_Symbol));
  // variable-sized types: small objects will be padded in blocks of
  // fixed size. the block size is the size of a flexible array with N
  // elements
  all_smallstring = blkalloc_init (SMALL_STRG_SIZE);
  all_smallvector = blkalloc_init (SMALL_VECT_SIZE);
  all_smalllambda = blkalloc_init (SMALL_LMBD_SIZE);

  // large variable-sized objects will go here
  lobheap = lobheap_init ();

  lastgcstats = (struct memstats){};
  gcgen = 0;
}

Lisp_Object
make_cons (Lisp_Object car, Lisp_Object cdr)
{
  Lisp_Cons *cons = blkalloc (all_cons);

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
      vec = blkalloc (all_smallvector);
    }
  else
    {
      // allocate in large object heap
      size_t allocsize = sizeof (Lisp_Vector) + size * sizeof (Lisp_Object);
      vec = loballoc (lobheap, allocsize);
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
      string = blkalloc (all_smallstring);
    }
  else
    {
      // allocate in large object heap (+1 for terminator)
      size_t allocsize = sizeof (Lisp_String) + (size + 1) * sizeof (char);
      string = loballoc (lobheap, allocsize);
    }

  string->size = size;
  // not counted in size
  string->data[size] = '\0';

  return box_string (string);
}

Lisp_Object
make_symbol (Lisp_Object name)
{
  Lisp_Symbol *symbol = blkalloc (all_symbol);

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
      lambda = blkalloc (all_smalllambda);
    }
  else
    {
      // allocate in large object heap
      size_t allocsize = sizeof (Lisp_Lambda) + maxargs * sizeof (Lisp_Object);
      lambda = loballoc (lobheap, allocsize);
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

void
free_lisp_obj (Lisp_Object o)
{
  if (o == LISP_NULL)
    return;

  if (type_of (o) == LISP_INTG)
    // integer is immediate, not a pointer. nothing to do
    return;

  free (unbox_pointer (o));
}

static int gcwait = 0;

int
gc_maybe ()
{
  // TODO ugly!!  remove this and use a counter on the blkalloc
  // this just to sample once in 20 instead of every time recalculating stats!
  if (gcwait++ < 20)
    return 0;
  gcwait = 0;

  // TODO defines
  const float growthreshold = 5.;
  const size_t minheap = PAGE_SIZE * 32;
  const size_t maxheap = 256 * 1024 * 1024;

  size_t used = current_used_size ();
  if (used > maxheap)
    {
      gc ();
      return 1;
    }

  size_t lastused = last_gcgen_used_size ();
  if (used > minheap && used > lastused * (1 + growthreshold))
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
  gcgen++;
  lastgcstats = stats;
  return stats;
}

static void
gcmarkobj (Lisp_Object obj)
{
  // TODO this marks ALL. awful. use three-color approach:
  //  black -> collect
  //  grey  -> working list
  //  white -> untouchable
  switch (type_of (obj))
    {
    case LISP_STRG:
      if (is_small_string (unbox_string (obj)->size))
        blkgcmark (all_smallstring, unbox_string (obj));
      else
        lobgcmark (lobheap, unbox_string (obj));
      break;
    case LISP_SYMB:
      if (!blkgcmark (all_symbol, unbox_symbol (obj)))
        break;
      gcmarkobj (unbox_symbol (obj)->name);
      gcmarkobj (unbox_symbol (obj)->value);
      break;
    case LISP_LMBD:
      if (is_small_lambda (unbox_lambda (obj)->maxargs))
        {
          if (!blkgcmark (all_smalllambda, unbox_lambda (obj)))
            break;
        }
      else
        {
          if (!lobgcmark (lobheap, unbox_lambda (obj)))
            break;
        }
      gcmarkobj (unbox_lambda (obj)->form);
      gcmarkobj (unbox_lambda (obj)->env);
      for (int i = 0; i < unbox_lambda (obj)->maxargs; i++)
        gcmarkobj (unbox_lambda (obj)->args[i]);
      break;
    case LISP_CONS:
      if (!blkgcmark (all_cons, unbox_cons (obj)))
        break;
      gcmarkobj (f_car (obj));
      // FIXME in case of a long list, this recursion could be FATAL.
      // replace this with explicit iteration on the cdr
      gcmarkobj (f_cdr (obj));
      break;
    case LISP_VECT:
      if (is_small_vector (unbox_vector (obj)->size))
        {
          if (!blkgcmark (all_smallvector, unbox_vector (obj)))
            break;
        }
      else
        {
          if (!lobgcmark (lobheap, unbox_vector (obj)))
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
  // TODO: let blkallocator take care of it by itself??
  blkgcsweep (all_cons);
  blkgcsweep (all_symbol);
  blkgcsweep (all_smallstring);
  blkgcsweep (all_smallvector);
  blkgcsweep (all_smalllambda);

  // sweep large object heap
  lobgcsweep (lobheap);

  return memstats ();
}

struct memstats
memstats ()
{
  return (struct memstats){
    .conses = blkstats (all_cons),
    .symbols = blkstats (all_symbol),
    .smallstrings = blkstats (all_smallstring),
    .smallvectors = blkstats (all_smallvector),
    .smalllambdas = blkstats (all_smalllambda),
    .loblength = lobheap->numblk,
    .lobsize = lobheap->heapsize,
  };
}

static size_t
sum_used_size (struct memstats stats)
{
  return stats.conses.sizeused + stats.symbols.sizeused
         + stats.smallstrings.sizeused + stats.smallvectors.sizeused
         + stats.smalllambdas.sizeused + lobheap->heapsize;
}

size_t
current_used_size ()
{
  return sum_used_size (memstats ());
}

size_t
last_gcgen_used_size ()
{
  return sum_used_size (lastgcstats);
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
  // TODO
  printf ("Fixed memory blocks:\n");
  size_t freesize = 0;
  /* struct blk *blk = all_cons->freelist; */
  /* while (blk) */
  /*   { */
  /*     freesize++; */
  /*     blk = blk->next; */
  /*   } */
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
  blkmemdump (all_cons);
  printf ("SYMBOL BLOCKS:\n");
  blkmemdump (all_symbol);
  printf ("SMALL STRING BLOCKS:\n");
  blkmemdump (all_smallstring);
  printf ("SMALL VECTOR BLOCKS:\n");
  blkmemdump (all_smallvector);
  printf ("SMALL LAMBDA BLOCKS:\n");
  blkmemdump (all_smalllambda);

  printf ("VAR SIZE HEAP:\n");
  lobmemdump (lobheap);
}
