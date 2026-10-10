#include "env.h"
#include "lisp.h"
#include "stack.h"
#include <stdio.h>

// just a marker we use as anchor for the env alist
#define ENV_ANCHOR box_int (42)

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
env_lookup_cell (Lisp_Object env, Lisp_Object symbol)
{
  return f_assq (symbol, env);
}

Lisp_Object
env_lookup (Lisp_Object env, Lisp_Object symbol)
{
  Lisp_Object cell = env_lookup_cell (env, symbol);
  return nil (cell) ? q_unbound : f_cdr (cell);
}

// TODO: this implementation makes multithreading impossible.
// TODO I don't want to keep this function here, don't want to import stack.
Lisp_Object
env_current ()
{
  return stack_current ().env;
}
