#include "env.h"
#include "alloc.h"
#include "eval.h"
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
env_new (Lisp_Object parent, Lisp_Object symbol, Lisp_Object value)
{
  Lisp_Object newcell = make_cons (symbol, value);
  return make_cons (newcell, parent);
}

Lisp_Object
env_lookup (Lisp_Object env, Lisp_Object symbol)
{
  Lisp_Object cell = f_assq (symbol, env);
  return nil (cell) ? q_unbound : f_cdr (cell);
}

// TODO: this implementation makes multithreading impossible.
Lisp_Object
env_current ()
{
  return stack_current ().env;
}
