#ifndef EVAL_H
#define EVAL_H

#include "lisp.h"
#include "stack.h"
#include <setjmp.h>

#define LMEND 0
#define LMCONTINUE 1

enum machinemode
{
  M_EVAL = 0,
  M_RETURN,
  M_APPLY,
};

struct lispmachine
{
  enum machinemode mode;
  union
  {
    // clang-format off
    struct { Lisp_Object env, exp;   } meval;
    struct { Lisp_Object proc, args; } mapply;
    struct { Lisp_Object val;        } mreturn;
    Lisp_Object registers[2];
    // clang-format on
  };
  struct stack *stack;

  // unit of work, term borrowed from erlang
  unsigned long reductions;

  // error handling
  jmp_buf jmp;
  Lisp_Object err;
};

struct lispmachine *lispmachine_init (Lisp_Object env, Lisp_Object form);
int lispmachine_step (struct lispmachine *lm);
Lisp_Object lispmachine_get_value (struct lispmachine *lm);
int lispmachine_handle (struct lispmachine *lm);
void lispmachine_regs_walk (struct lispmachine *lm, void (*fun) (Lisp_Object));
void lispmachine_stack_walk (struct lispmachine *lm, void (*fun) (struct stackframe));

// entrypoints for single-machine language, to be removed in the future
int eval (Lisp_Object env, Lisp_Object form, Lisp_Object *out);
int eval_toplevel (Lisp_Object form, Lisp_Object *out);
void lispmachine_cur_regs_walk (void (*fun) (Lisp_Object));
void lispmachine_cur_stack_walk (void (*fun) (struct stackframe));

NORETURN void xsignal (Lisp_Object symbol, Lisp_Object data);
NORETURN void xsignal_err (Lisp_Object err);

static inline void
goto_eval (struct lispmachine *lm, Lisp_Object env, Lisp_Object exp)
{
  lm->mode = M_EVAL;
  lm->meval.env = env;
  lm->meval.exp = exp;
}

static inline void
goto_apply (struct lispmachine *lm, Lisp_Object proc, Lisp_Object args)
{
  lm->mode = M_APPLY;
  lm->mapply.proc = proc;
  lm->mapply.args = args;
}

static inline void
goto_return (struct lispmachine *lm, Lisp_Object val)
{
  lm->mode = M_RETURN;
  lm->mreturn.val = val;
  lm->registers[1] = q_nil;
}

#endif /* EVAL_H */
