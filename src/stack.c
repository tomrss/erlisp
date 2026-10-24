#include "stack.h"
#include "alloc.h"
#include "error.h"
#include "lisp.h"
#include <stddef.h>

static struct stack *stack;

static struct stackpage *
page_new (struct stack *s)
{
  struct stackpage *page = malloc (sizeof (struct stackpage));
  // TODO check malloc
  page->depth = 0;
  page->next = s->pages;
  s->pages = page;
  s->numpages++;
  return page;
}

void
stack_init ()
{
  stack = malloc (sizeof (struct stack));
  // TODO check malloc
  stack->numpages = 0;
  stack->pages = NULL;
  page_new (stack);
}

void
stack_push (struct stackframe sf)
{
  struct stack *s = stack; // to ease refactoring later, this will be arg

  if (s->pages->depth >= STACKPAGESIZE)
    {
      if (s->numpages >= STACKMAXPAGES)
        stackoverflow_error ();

      page_new (s);
    }

  s->pages->frames[s->pages->depth++] = sf;
}

void
stack_push_halt (Lisp_Object env)
{
  struct stackframe sf = {
    .kind = F_HALT,
    .env = env,
  };
  sf.slots[0] = sf.slots[1] = q_nil;
  stack_push (sf);
}

void
stack_push_seq (Lisp_Object env, Lisp_Object rest)
{
  struct stackframe sf = {
    .kind = F_SEQ,
    .env = env,
    .seq = { .rest = rest },
  };
  sf.slots[1] = q_nil;
  stack_push (sf);
}

void
stack_push_args (Lisp_Object env, Lisp_Object argl, Lisp_Object unev)
{
  struct stackframe sf = {
    .kind = F_ARGS,
    .env = env,
    .args = { .argl = argl, .unev = unev },
  };
  stack_push (sf);
}

void
stack_push_if (Lisp_Object env, Lisp_Object body)
{
  struct stackframe sf = {
    .kind = F_IF,
    .env = env,
    .if_ = { .body = body },
  };
  sf.slots[1] = q_nil;
  stack_push (sf);
}

void
stack_push_define (Lisp_Object env, Lisp_Object symbol)
{
  struct stackframe sf = {
    .kind = F_DEFINE,
    .env = env,
    .define = { .symbol = symbol },
  };
  sf.slots[1] = q_nil;
  stack_push (sf);
}

void
stack_push_setq (Lisp_Object env, Lisp_Object symbol)
{
  struct stackframe sf = {
    .kind = F_SETQ,
    .env = env,
    .setq = { .symbol = symbol },
  };
  sf.slots[1] = q_nil;
  stack_push (sf);
}

void
stack_push_conditioncase (Lisp_Object env, Lisp_Object errsym,
                          Lisp_Object handlers)
{
  struct stackframe sf = {
    .kind = F_CONDITION_CASE,
    .env = env,
    .conditioncase = { .handlers = handlers, .errsym = errsym },
  };
  stack_push (sf);
}

void
stack_push_unwindprotect (Lisp_Object env, Lisp_Object cleanups)
{
  struct stackframe sf = {
    .kind = F_UNWIND_PROTECT,
    .env = env,
    .unwindprotect = { .cleanups = cleanups },
  };
  sf.slots[1] = q_nil;
  stack_push (sf);
}

void
stack_push_restore (Lisp_Object env, Lisp_Object val)
{
  struct stackframe sf = {
    .kind = F_RESTORE,
    .env = env,
    .restore = { .val = val },
  };
  sf.slots[1] = q_nil;
  stack_push (sf);
}

void
stack_push_reraise (Lisp_Object env, Lisp_Object err)
{
  struct stackframe sf = {
    .kind = F_RERAISE,
    .env = env,
    .reraise = { .err = err },
  };
  sf.slots[1] = q_nil;
  stack_push (sf);
}

struct stackframe
stack_pop ()
{
  struct stack *s = stack; // to ease refactoring later, this will be arg

  if (s->pages->depth == 0 && s->numpages == 1)
    internal_error ("Unable to pop beginning of stack");

  struct stackframe sf = s->pages->frames[--s->pages->depth];

  if (s->pages->depth == 0 && s->numpages > 1)
    {
      struct stackpage *emptypage = s->pages;
      s->pages = s->pages->next;
      s->numpages--;
      free (emptypage);
    }

  return sf;
}

size_t
stack_depth_current ()
{
  struct stack *s = stack; // to ease refactoring later, this will be arg
  return (s->numpages - 1) * STACKPAGESIZE + s->pages->depth;
}

void
stack_walk (void (*fun) (struct stackframe))
{
  struct stack *s = stack; // to ease refactoring later, this will be arg

  struct stackpage *page = s->pages;
  while (page != NULL)
    {
      for (int i = page->depth - 1; i >= 0; i--)
        fun (page->frames[i]);

      page = page->next;
    }
}

Lisp_Object
stack_backtrace (UNUSED int depthfrom)
{
  Lisp_Object backtrace = q_nil;

  // TODO: commented out code; with the new evaluator with lispmachine
  // i still don't know how to do it. i will think of it in the future.
  /* Lisp_Object tail = q_nil; */
  /* int depth = stack_depth_current () - 1; */
  /* struct stackpage *page = s->pages; */
  /* while (page != NULL) */
  /*   { */
  /*     for (int i = page->depth - 1; i >= 0; i--) */
  /*       { */
  /*         if (depth-- < depthfrom) */
  /*           break; */

  /*         struct stackframe sf = page->frames[i]; */
  /*         Lisp_Object cell = f_cons (make_string (sf.fname), q_nil); */
  /*         if (nil (backtrace)) */
  /*           backtrace = cell; */
  /*         else */
  /*           f_setcdr (tail, cell); */
  /*         tail = cell; */
  /*       } */

  /*     page = page->next; */
  /*   } */

  return backtrace;
}
