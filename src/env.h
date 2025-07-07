#ifndef ENV_H
#define ENV_H

#include "lisp.h"

#define STACKSIZE 1024

struct stackframe
{
  const char *fname;
  Lisp_Object env;
};

Lisp_Object env_init ();
Lisp_Object env_new (Lisp_Object parent, Lisp_Object symbol);
Lisp_Object env_lookup (Lisp_Object env, Lisp_Object symbol);
Lisp_Object env_lookup_name (Lisp_Object env, Lisp_Object name);
Lisp_Object env_current ();

void stack_push (struct stackframe sf);
struct stackframe stack_pop ();
struct stackframe stack_pop_free ();
struct stackframe stack_peek ();
void stack_current_set_env (Lisp_Object env);

#endif /* ENV_H */
