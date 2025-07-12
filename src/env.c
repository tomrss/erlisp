#include "env.h"
#include "alloc.h"
#include "lisp.h"
#include <stdio.h>

// TODO confusing function: like multiple envs could be init this way, not
// true.
Lisp_Object
env_init ()
{
  Lisp_Object env = q_nil;
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
  return stack_current ().env;
}
