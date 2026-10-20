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
stack_init (Lisp_Object baseenv)
{
  stack = malloc (sizeof (struct stack));
  // TODO check malloc
  stack->numpages = 0;
  stack->pages = NULL;
  page_new (stack);
  stack_push ((struct stackframe){ .fname = "base", .env = baseenv });
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

struct stackframe
stack_current ()
{
  struct stack *s = stack; // to ease refactoring later, this will be arg

  return s->pages->frames[s->pages->depth - 1];
}

struct stackframe *
stack_current_ref ()
{
  struct stack *s = stack; // to ease refactoring later, this will be arg

  return &s->pages->frames[s->pages->depth - 1];
}

size_t
stack_depth_current ()
{
  struct stack *s = stack; // to ease refactoring later, this will be arg
  return (s->numpages - 1) * STACKPAGESIZE + s->pages->depth;
}

void
stack_current_set_env (Lisp_Object env)
{
  struct stack *s = stack; // to ease refactoring later, this will be arg
  s->pages->frames[s->pages->depth - 1].env = env;
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

void
stack_unwind (int depthfrom, void (*fun) (struct stackframe *))
{
  while (stack_depth_current () > (size_t)depthfrom)
    {
      struct stackframe *sf = stack_current_ref ();
      if (fun != NULL)
        fun (sf);

      // pop only after func invocation for correct gc roots during exec of fun
      stack_pop ();
    }
}

Lisp_Object
stack_backtrace (int depthfrom)
{
  Lisp_Object backtrace = q_nil;
  Lisp_Object tail = q_nil;

  struct stack *s = stack; // to ease refactoring later, this will be arg

  int depth = stack_depth_current () - 1;
  struct stackpage *page = s->pages;
  while (page != NULL)
    {
      for (int i = page->depth - 1; i >= 0; i--)
        {
          if (depth-- < depthfrom)
            break;

          struct stackframe sf = page->frames[i];
          Lisp_Object cell = f_cons (make_string (sf.fname), q_nil);
          if (nil (backtrace))
            backtrace = cell;
          else
            f_setcdr (tail, cell);
          tail = cell;
        }

      page = page->next;
    }

  return backtrace;
}
