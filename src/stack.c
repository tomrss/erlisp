#include "stack.h"
#include "error.h"
#include "lisp.h"

struct stackframe stack[STACKSIZE];
int stackdepth = 0;

void
stack_init (Lisp_Object baseenv)
{
  stack_push ((struct stackframe){ .fname = "base", .env = baseenv });
}

void
stack_push (struct stackframe sf)
{
  if (stackdepth >= STACKSIZE)
    stackoverflow_error ();

  stack[stackdepth++] = sf;
}

struct stackframe
stack_pop ()
{
  if (stackdepth <= 0)
    {
      internal_error ("Unable to pop beginning of stack");
    }

  return stack[--stackdepth];
}

struct stackframe
stack_current ()
{
  return stack[stackdepth - 1];
}

int
stack_depth_current ()
{
  return stackdepth;
}

void
stack_current_set_env (Lisp_Object env)
{
  stack[stackdepth - 1].env = env;
}

void
stack_walk (void (*fun) (struct stackframe))
{
  for (int i = stackdepth - 1; i >= 0; i--)
    fun (stack[i]);
}

Lisp_Object
stack_unwind (int depthfrom, void (*fun) (struct stackframe))
{
  Lisp_Object backtrace = q_nil;
  Lisp_Object tail = q_nil;

  while (stackdepth > depthfrom)
    {
      struct stackframe sf = stack_pop ();

      if (fun != NULL)
        fun (sf);

      Lisp_Object cell = f_cons (make_string (sf.fname), q_nil);

      if (eq (backtrace, q_nil))
        backtrace = cell;
      else
        f_setcdr (tail, cell);

      tail = cell;
    }

  return backtrace;
}
