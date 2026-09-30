#ifndef EVAL_H
#define EVAL_H

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
};

void stack_push (struct stackframe sf);
struct stackframe stack_pop ();
struct stackframe stack_pop_free ();
struct stackframe stack_current ();
void stack_walk (void (*fun) (struct stackframe));
void stack_current_set_env (Lisp_Object env);
void stack_parent_set_env (Lisp_Object env);

Lisp_Object eval (Lisp_Object env, Lisp_Object form);
Lisp_Object eval_symbol (Lisp_Object env, Lisp_Object form);
Lisp_Object call_function (Lisp_Object env, Lisp_Object form);
Lisp_Object call_subr (Lisp_Subr *usubr, int maxargs, int arity,
                       Lisp_Object *argvals);
Lisp_Object call_unevalled_subr (Lisp_Subr *usubr, Lisp_Object form);
Lisp_Object call_lambda (Lisp_Object env, Lisp_Lambda *ulambda,
                         Lisp_Object *argvals);
Lisp_Object progn (Lisp_Object env, Lisp_Object form);
Lisp_Object let (Lisp_Object env, Lisp_Object form);
Lisp_Object define (Lisp_Object env, Lisp_Object form);

NORETURN void xsignal (Lisp_Object symbol, Lisp_Object data);

int condition_case_0 (Lisp_Object (*fun) (), Lisp_Object *out);
int condition_case_1 (Lisp_Object (*fun) (Lisp_Object), Lisp_Object arg1,
                      Lisp_Object *out);
int condition_case_2 (Lisp_Object (*fun) (Lisp_Object, Lisp_Object),
                      Lisp_Object arg1, Lisp_Object arg2, Lisp_Object *out);
int condition_case_n (Lisp_Object (*fun) (int, Lisp_Object *), int nargs,
                      Lisp_Object *args, Lisp_Object *out);

int safe_eval (Lisp_Object env, Lisp_Object form, Lisp_Object *out);

#endif /* EVAL_H */
