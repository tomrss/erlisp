#ifndef EVAL_H
#define EVAL_H

#include "lisp.h"

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

#endif /* EVAL_H */
