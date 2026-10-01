#include "env.h"
#include "eval.h"
#include "lisp.h"
#include <stdio.h>

// just a marker we use as anchor for the env alist
#define ENV_ANCHOR box_int (42)

// TODO confusing function: like multiple envs could be init this way, not
// true.
Lisp_Object
env_init ()
{
  Lisp_Object env = f_cons (ENV_ANCHOR, q_nil);
  stack_push ((struct stackframe){ .fname = "base", .env = env });
  return env;
}

Lisp_Object
env_new (Lisp_Object parent)
{
  return f_cons (ENV_ANCHOR, f_cdr (parent));
}

Lisp_Object
env_define (Lisp_Object env, Lisp_Object symbol, Lisp_Object value)
{
  Lisp_Object newcell = f_cons (symbol, value);
  // put new cell at the head of alist after the anchor
  Lisp_Object envnoanchor = f_cdr (env);
  f_setcdr (env, f_cons (newcell, envnoanchor));
  return env;
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
