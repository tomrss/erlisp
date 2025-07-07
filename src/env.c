#include "env.h"
#include "alloc.h"
#include "debug.h"
#include "lisp.h"
#include <stdio.h>

struct stackframe stack[STACKSIZE];
int stackind = 0;

// TODO confusing function: like multiple envs could be init this way, not
// true.
Lisp_Object
env_init ()
{
  Lisp_Object env = make_cons (q_nil, q_nil);
  stack_push ((struct stackframe){ .fname = "base", .env = env });
  return env;
}

Lisp_Object
env_new (Lisp_Object parent, Lisp_Object symbol)
{
  Lisp_Object newcell = make_cons (unbox_symbol (symbol)->name, symbol);
  return make_cons (newcell, parent);
}

Lisp_Object
env_lookup (Lisp_Object env, Lisp_Object symbol)
{
  // TODO type safety
  return env_lookup_name (env, unbox_symbol (symbol)->name);
}

Lisp_Object
env_lookup_name (Lisp_Object env, Lisp_Object name)
{
  // TODO type safety
  return f_cdr (f_assoc (name, env));
}

// TODO: this implementation makes multithreading impossible.
Lisp_Object
env_current ()
{
  return stack_peek ().env;
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
stack_peek ()
{
  return stack[stackind];
}

void
stack_current_set_env (Lisp_Object env)
{
  stack[stackind].env = env;
}

struct stackframe
stack_pop_free ()
{
  struct stackframe pop = stack_pop ();
  struct stackframe cur = stack_peek ();

  Lisp_Object tail = pop.env;
  Lisp_Object target = cur.env;

  while (!eq (tail, target) && !eq (tail, q_nil))
    {
      free_lisp_obj (f_car (tail));
      tail = f_cdr (tail);
    }

  return pop;
}
