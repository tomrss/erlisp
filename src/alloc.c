#include "alloc.h"
#include "blkalloc.h"
#include "debug.h"
#include "env.h"
#include "lisp.h"
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

struct stackframe stack[STACKSIZE];
int stackind = 0;

blkallocator *all_cons;
blkallocator *all_symbol;
blkallocator *all_smallstring;
blkallocator *all_smallvector;
blkallocator *all_smalllambda;

struct varsizeblk
{
  Lisp_Object obj;
  struct varsizeblk *next;
};

struct varsizeblk *varsizeheap;
unsigned long int varsizeheaplength;
size_t varsizeheapsize;

static int is_obj_unmarked (Lisp_Object obj);
static int is_cons_unmarked (void *ptr);
static int is_symbol_unmarked (void *ptr);
static int is_string_unmarked (void *ptr);
static int is_vector_unmarked (void *ptr);
static int is_lambda_unmarked (void *ptr);
static void unmark_obj (Lisp_Object obj);
static void unmark_cons (void *ptr);
static void unmark_symbol (void *ptr);
static void unmark_string (void *ptr);
static void unmark_vector (void *ptr);
static void unmark_lambda (void *ptr);

static void gcmarkobj (Lisp_Object obj);
static void gcmark ();
static struct memstats gcsweep ();
static void gcunmark ();

void
init_alloc ()
{
  // fixed-sized types: block size is the size of struct
  all_cons = blkalloc_init (sizeof (Lisp_Cons), is_cons_unmarked);
  all_symbol = blkalloc_init (sizeof (Lisp_Symbol), is_symbol_unmarked);
  // variable-sized types: small objects will be padded in blocks of
  // fixed size. the block size is the size of a flexible array with N
  // elements
  all_smallstring = blkalloc_init (SMALL_STRG_SIZE, is_string_unmarked);
  all_smallvector = blkalloc_init (SMALL_VECT_SIZE, is_vector_unmarked);
  all_smalllambda = blkalloc_init (SMALL_LMBD_SIZE, is_lambda_unmarked);
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
  if (size < SMALL_VECT_NELTS)
    {
      // allocate as fixed size holding SMALL_VECT_SIZE elements
      vec = blkalloc (all_smallvector);
    }
  else
    {
      // allocate in variable sized heap
      size_t allocsize = sizeof (Lisp_Vector) + size * sizeof (Lisp_Object);
      vec = malloc (allocsize);
      struct varsizeblk *blk = malloc (sizeof (struct varsizeblk));
      blk->obj = box_vector (vec);
      blk->next = varsizeheap;
      varsizeheap = blk;
      varsizeheaplength++;
      varsizeheapsize += allocsize;
    }

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
  Lisp_String *string;
  if (size < SMALL_STRG_NCHRS)
    {
      // allocate as fixed sized (with some padding)
      string = blkalloc (all_smallstring);
    }
  else
    {
      // allocate in variable sized heap
      size_t allocsize = sizeof (Lisp_String) + size * sizeof (char);
      string = malloc (allocsize);
      struct varsizeblk *blk = malloc (sizeof (struct varsizeblk));
      blk->obj = box_string (string);
      blk->next = varsizeheap;
      varsizeheap = blk;
      varsizeheaplength++;
      varsizeheapsize += allocsize;
    }

  string->gcmark = 0;
  string->size = size;
  strncpy (string->data, s, size);

  return box_string (string);
}

Lisp_Object
make_symbol (Lisp_Object name)
{
  Lisp_Symbol *symbol = blkalloc (all_symbol);

  symbol->name = name;
  symbol->value = q_unbound;
  symbol->next = NULL;
  symbol->gcmark = 0;

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

  subr->name = name; // TODO probably safer to copy name
  subr->function = fun;
  subr->minargs = minargs;
  subr->maxargs = maxargs;

  return box_subr (subr);
}

Lisp_Object
make_lambda (int minargs, int maxargs, Lisp_Object *args, Lisp_Object form)
{
  Lisp_Lambda *lambda;
  // TODO maybe use a lisp list args instead of c array?
  if (maxargs <= SMALL_LMBD_NARGS)
    {
      lambda = blkalloc (all_smallvector);
    }
  else
    {
      // allocate in variable sized heap
      size_t allocsize = sizeof (Lisp_Lambda) + maxargs * sizeof (Lisp_Object);
      lambda = malloc (allocsize);
      struct varsizeblk *blk = malloc (sizeof (struct varsizeblk));
      blk->obj = box_lambda (lambda);
      blk->next = varsizeheap;
      varsizeheap = blk;
      varsizeheaplength++;
      varsizeheapsize += allocsize;
    }

  lambda->minargs = minargs;
  lambda->maxargs = maxargs;
  for (int i = 0; i < maxargs; i++)
    lambda->args[i] = args[i];
  lambda->form = form;
  lambda->gcmark = 0;

  return box_lambda (lambda);
}

Lisp_Object
defsubr (const char *name, int minargs, int maxargs, union lisp_subr_fun fun)
{
  Lisp_Object subr = make_subr (name, minargs, maxargs, fun);
  Lisp_Object symb = make_str_symbol (name);
  {
    // TODO implement pure storage and remove this HORROR
    unbox_symbol (symb)->gcmark = 1;
    unbox_string (unbox_symbol (symb)->name)->gcmark = 1;
  }
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

void
stack_push (struct stackframe sf)
{
  if (stackind >= STACKSIZE)
    {
      // TODO err
      fprintf (stderr, "stack size exceeded: %d\n", STACKSIZE);
      exit (9);
    }

  stack[++stackind] = sf;
}

struct stackframe
stack_pop ()
{
  if (stackind < 0)
    {
      // TODO err
      fprintf (stderr, "already at beginning of stack\n");
      exit (9);
    }

  return stack[--stackind];
}

struct stackframe
stack_current ()
{
  return stack[stackind];
}

void
stack_parent_set_env (Lisp_Object env)
{
  int effind = stackind > 0 ? stackind - 1 : 0;

  stack[effind].env = env;
}

void
stack_current_set_env (Lisp_Object env)
{
  // TODO unmark gc?
  stack[stackind].env = env;
}

struct stackframe
stack_pop_free ()
{
  struct stackframe pop = stack_pop ();
  /* struct stackframe cur = stack_current (); */

  /* Lisp_Object tail = pop.env; */
  /* Lisp_Object target = cur.env; */

  // TODO we cannot really free as they are managed by gc
  /* while (!eq (tail, target) && !eq (tail, q_nil)) */
  /*   { */
  /*     free_lisp_obj (f_car (tail)); */
  /*     tail = f_cdr (tail); */
  /*   } */

  return pop;
}

struct memstats
gc ()
{
  struct memstats stats;
  gcmark ();
  stats = gcsweep ();
  gcunmark ();
  return stats;
}

static void
gcmarkobj (Lisp_Object obj)
{
  if (eq (obj, q_nil) || eq (obj, q_t) || eq (obj, q_unbound))
    return;

  // TODO this marks ALL. awful. use three-color approach:
  //  black -> collect
  //  grey  -> working list
  //  white -> untouchable
  switch (type_of (obj))
    {
    case LISP_STRG:
      if (unbox_string (obj)->gcmark == 1)
        break;
      unbox_string (obj)->gcmark = 1;
      break;
    case LISP_SYMB:
      if (unbox_symbol (obj)->gcmark == 1)
        break;
      unbox_symbol (obj)->gcmark = 1;
      gcmarkobj (unbox_symbol (obj)->name);
      gcmarkobj (unbox_symbol (obj)->value);
      break;
    case LISP_LMBD:
      if (unbox_lambda (obj)->gcmark == 1)
        break;
      unbox_lambda (obj)->gcmark = 1;
      // TODO arg list as lisp list? -> add here gcmark of that
      gcmarkobj (unbox_lambda (obj)->form);
      for (int i = 0; i < unbox_lambda (obj)->maxargs; i++)
        gcmarkobj (unbox_lambda (obj)->args[i]);
      break;
    case LISP_CONS:
      // TODO
      if (unbox_cons (obj)->gcmark == 1)
        break;
      unbox_cons (obj)->gcmark = 1;
      gcmarkobj (f_car (obj));
      gcmarkobj (f_cdr (obj));
      break;
    case LISP_VECT:
      if (unbox_vector (obj)->gcmark == 1)
        break;
      unbox_vector (obj)->gcmark = 1;
      break;
    case LISP_INTG:
    case LISP_SUBR:
      break;
    }
}

static void
gcmark ()
{
  gcmarkobj (env_current ());

  // TODO: obarray symbols should be protected from gc in other
  // way. maybe definining a "pure lisp" memory like in Emacs Lisp
  // where predefined objects are allocated and safe from gc
  //
  // Or maybe obarray should be dropped and just use env.
  gcmarkobj(v_obarray);
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
  blkgc (all_cons);
  blkgc (all_symbol);
  blkgc (all_smallstring);
  blkgc (all_smallvector);
  blkgc (all_smalllambda);

  // sweep variable sized heap
  struct varsizeblk *blk = varsizeheap;
  struct varsizeblk *last = NULL;
  struct varsizeblk *next;
  while (blk)
    {
      next = blk->next;
      if (is_obj_unmarked (blk->obj))
        {
          size_t freesize = sizeof (unbox_pointer (blk->obj));
          free_lisp_obj (blk->obj);

          // pop from var size heap
          if (last)
            // pop from middle
            last->next = next;
          else
            // pop from start
            varsizeheap = blk->next;
          blk->next = NULL;

          varsizeheaplength--;
          varsizeheapsize -= freesize;
        }
      else
        {
          last = blk;
        }
      blk = next;
    }

  return memstats ();
}

static void
gcunmark ()
{
  // unmark fixed block memory
  blkwalk (all_cons, unmark_cons);
  blkwalk (all_symbol, unmark_symbol);
  blkwalk (all_smallstring, unmark_string);
  blkwalk (all_smallvector, unmark_vector);
  blkwalk (all_smalllambda, unmark_lambda);

  // unmark variable sized heap
  struct varsizeblk *blk = varsizeheap;
  while (blk)
    {
      unmark_obj (blk->obj);
      blk = blk->next;
    }
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
    .varsizeheaplength = varsizeheaplength,
    .varsizeheapsize = varsizeheapsize,
  };
}

static void
print_blkmemstats (const char *name, blkmemstats st)
{
  printf (" %-14s: %3lu pages (%6zu B), %4lu free (%6zu B), %4lu used (%6zu "
          "B)\n",
          name, st.numpages, st.sizepages, st.numfree, st.sizefree, st.numused,
          st.sizeused);
};

void
print_memstats (struct memstats stats)
{
  printf ("Fixed memory blocks:\n");
  size_t freesize = 0;
  struct blk *blk = all_cons->freelist;
  while (blk)
    {
      freesize++;
      blk = blk->next;
    }
  printf ("real cons freelist size: %zu\n", freesize);
  print_blkmemstats ("conses", stats.conses);
  print_blkmemstats ("symbols", stats.symbols);
  print_blkmemstats ("small strings", stats.smallstrings);
  print_blkmemstats ("small vectors", stats.smallvectors);
  print_blkmemstats ("small lambda", stats.smalllambdas);
  printf ("Variable sized heap:\n");
  printf (" %lu objects (%zu B)\n", stats.varsizeheaplength,
          stats.varsizeheapsize);
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
  int i = 0;
  struct varsizeblk *blk = varsizeheap;
  while (blk)
    {
      printf (" %3d: blk %-14p, next %-14p, obj %-14p, %s\n", i, blk, blk->next,
              unbox_pointer(blk->obj), type_name (type_of (blk->obj)));
      i++;
      blk = blk->next;
    }
}

static void
unmark_obj (Lisp_Object obj)
{
  switch (type_of (obj))
    {
    case LISP_STRG:
      unmark_string (unbox_string (obj));
      break;
    case LISP_SYMB:
      unmark_symbol (unbox_symbol (obj));
      break;
    case LISP_CONS:
      unmark_cons (unbox_cons (obj));
      break;
    case LISP_VECT:
      unmark_vector (unbox_vector (obj));
      break;
    case LISP_LMBD:
      unmark_lambda (unbox_lambda (obj));
      break;
    case LISP_INTG:
    case LISP_SUBR:
      break;
    }
}

static void
unmark_cons (void *ptr)
{
  if (!ptr)
    return;
  Lisp_Cons *l = ptr;
  l->gcmark = 0;
}

static void
unmark_symbol (void *ptr)
{
  if (!ptr)
    return;
  Lisp_Symbol *l = ptr;
  l->gcmark = 0;
}

static void
unmark_string (void *ptr)
{
  if (!ptr)
    return;
  Lisp_String *l = ptr;
  l->gcmark = 0;
}

static void
unmark_vector (void *ptr)
{
  if (!ptr)
    return;
  Lisp_Vector *l = ptr;
  l->gcmark = 0;
}

static void
unmark_lambda (void *ptr)
{
  if (!ptr)
    return;
  Lisp_Lambda *l = ptr;
  l->gcmark = 0;
}

static int
is_obj_unmarked (Lisp_Object obj)
{
  switch (type_of (obj))
    {
    case LISP_INTG:
    case LISP_SUBR:
      return 0;
    case LISP_STRG:
      return is_string_unmarked (unbox_string (obj));
    case LISP_SYMB:
      return is_symbol_unmarked (unbox_symbol (obj));
    case LISP_CONS:
      return is_cons_unmarked (unbox_cons (obj));
    case LISP_VECT:
      return is_vector_unmarked (unbox_vector (obj));
    case LISP_LMBD:
      return is_lambda_unmarked (unbox_lambda (obj));
    default:
      return -1;
    }
}

static int
is_cons_unmarked (void *ptr)
{
  if (!ptr)
    return 1;
  Lisp_Cons *l = ptr;
  return l->gcmark == 0;
}

static int
is_symbol_unmarked (void *ptr)
{
  if (!ptr)
    return 0;
  Lisp_Symbol *l = ptr;
  // todo these shouldn't even be here but in protected area from gc
  if (l == unbox_symbol (q_unbound))
    return 0;
  if (l == unbox_symbol (q_nil))
    return 0;
  if (l == unbox_symbol (q_t))
    return 0;

  return l->gcmark == 0;
}

static int
is_string_unmarked (void *ptr)
{
  if (!ptr)
    return 0;
  Lisp_String *l = ptr;
  return l->gcmark == 0;
}

static int
is_vector_unmarked (void *ptr)
{
  if (!ptr)
    return 0;
  Lisp_Vector *l = ptr;
  return l->gcmark == 0;
}

static int
is_lambda_unmarked (void *ptr)
{
  if (!ptr)
    return 0;
  Lisp_Lambda *l = ptr;
  return l->gcmark == 0;
}
