#ifndef STACK_H
#define STACK_H

#include "lisp.h"

#define STACKSIZE 1024

struct stackframe
{
  const char *fname;
  Lisp_Object env;
  // code in evaluation for protecting it from gc
  Lisp_Object form;
  Lisp_Object *argvals;
  int nargs;
  // unwind protect
  Lisp_Object cleanups;
  Lisp_Object keepalive; // simple slot for preserving somehting from gc
};

void stack_init (Lisp_Object baseenv);
void stack_push (struct stackframe sf);
struct stackframe stack_pop ();
struct stackframe stack_current ();
struct stackframe *stack_current_ref (); // TODO not a big fan of this...
int stack_depth_current ();
void stack_current_set_env (Lisp_Object env);
void stack_walk (void (*fun) (struct stackframe));
void stack_unwind (int depthfrom, void (*fun) (struct stackframe * ));
Lisp_Object stack_backtrace (int depthfrom);

#endif /* STACK_H */
