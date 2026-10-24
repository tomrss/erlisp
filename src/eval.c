#include "eval.h"
#include "alloc.h"
#include "env.h"
#include "error.h"
#include "lisp.h"
#include "print.h"
#include "stack.h"
#include <setjmp.h>

static void step_eval (struct lispmachine *lm);
static void step_apply (struct lispmachine *lm);
static int step_return (struct lispmachine *lm);

static int try_inline_eval (Lisp_Object env, Lisp_Object form,
                            Lisp_Object *out);

static Lisp_Object find_binding (Lisp_Object env, Lisp_Object symbol);
static Lisp_Object call_subr (Lisp_Subr *usubr, int maxargs, int arity,
                              Lisp_Object *argvals);
static void eval_application (struct lispmachine *lm, Lisp_Object env,
                              Lisp_Object form);
static void eval_body (struct lispmachine *lm, Lisp_Object env,
                       Lisp_Object tail);
static void eval_lambda (struct lispmachine *lm, Lisp_Object env,
                         Lisp_Object form);

static void return_if (struct lispmachine *lm, Lisp_Object env,
                       Lisp_Object cond, Lisp_Object body);

static void return_define (struct lispmachine *lm, Lisp_Object env,
                           Lisp_Object symbol, Lisp_Object value);

static void return_setq (struct lispmachine *lm, Lisp_Object env,
                         Lisp_Object symbol, Lisp_Object value);

static void return_args (struct lispmachine *lm, Lisp_Object env,
                         Lisp_Object value, Lisp_Object argl,
                         Lisp_Object unev);

struct lispmachine *
lispmachine_init (Lisp_Object env, Lisp_Object form)
{
  struct lispmachine *lm = malloc (sizeof (struct lispmachine));
  lm->stack = NULL; // TODO for now stack is global
  lm->reductions = 0;
  lm->err = q_nil;
  goto_eval (lm, env, form);
  stack_push_halt (env);
  return lm;
};

int
lispmachine_step (struct lispmachine *lm)
{
  gc_maybe ();
  switch (lm->mode)
    {
    case M_EVAL:
      step_eval (lm);
      return LMCONTINUE;
    case M_APPLY:
      step_apply (lm);
      return LMCONTINUE;
    case M_RETURN:
      return step_return (lm);
    }

  return LMEND;
}

Lisp_Object
lispmachine_get_value (struct lispmachine *lm)
{
  if (lm->mode != M_RETURN)
    // TODO this check is OBVIOUSLY not sufficient! not a problem for now
    internal_error ("cannot get value from running machine");
  return lm->mreturn.val;
}

int
lispmachine_handle (struct lispmachine *lm)
{
  Lisp_Object err = lm->err;
  Lisp_Object errsym = f_car (err);

  struct stackframe frame;
  while ((frame = stack_pop ()).kind != F_HALT)
    {
      switch (frame.kind)
        {
        case F_CONDITION_CASE:
          {
            Lisp_Object errtobind = frame.conditioncase.errsym;
            Lisp_Object conds = frame.conditioncase.handlers;

            while (!nil (conds))
              {
                Lisp_Object cond = f_car (conds);
                Lisp_Object condsym = f_car (cond);

                if (eq (condsym, q_t) ||    // catch all
                    eq (errsym, condsym) || // match single
                    (consp (condsym)
                     && !nil (f_memq (errsym, condsym)))) // match in list
                  {
                    Lisp_Object errenv = env_new (frame.env);
                    env_define (errenv, errtobind, err);

                    Lisp_Object condbody = f_cdr (cond);
                    eval_body (lm, errenv, condbody);
                    return 1;
                  }

                conds = f_cdr (conds);
              }
          }
          break;
        case F_UNWIND_PROTECT:
          stack_push_reraise (frame.env, err);
          eval_body (lm, frame.env, frame.unwindprotect.cleanups);
          return 1;
        default:
          // nothing to do with this frame
          break;
        }
    }

  // not handled
  return 0;
}

// TODO this is TEMPORARY and any usage of it is as well
static struct lispmachine *currentlm;

int
eval_toplevel (Lisp_Object form, Lisp_Object *out)
{
  // the entry point from C (repl, main, prelude). eval is defined in lisp
  // by the prelude and expands macros first: until then, evaluate without
  // expanding
  Lisp_Object fun = unbox_symbol (q_eval)->value;
  if (!unbound (fun))
    {
      // build form "(eval 'form)" with eval being the one defined in lisp
      form = f_cons (q_quote, f_cons (form, q_nil));
      form = f_cons (q_eval, f_cons (form, q_nil));
    }

  return eval (l_globalenv, form, out);
}

int
eval (Lisp_Object env, Lisp_Object form, Lisp_Object *out)
{
  // this is after refactoring with register lisp machine evaluator.
  // this is TEMPORARY to return to a working state for the language.
  // the final implementation will involve multiple lisp machines,
  // concurrent processes, scheduler, ...

  struct lispmachine *lm = lispmachine_init (env, form);
  currentlm = lm;

  if (setjmp (lm->jmp))
    {
      if (!lispmachine_handle (lm))
        {
          // unhandled error
          *out = lm->err;
          currentlm = NULL;
          free (lm);
          return 0;
        }
    } // no else: just resume machine steps

  while (lispmachine_step (lm))
    ;

  Lisp_Object result = lispmachine_get_value (lm);
  currentlm = NULL;
  free (lm);
  *out = result;
  return 1;
}

void
lispmachine_cur_regs_walk (void (*fun) (Lisp_Object))
{
  if (currentlm == NULL)
    return;
  lispmachine_regs_walk (currentlm, fun);
}

void
lispmachine_regs_walk (struct lispmachine *lm, void (*fun) (Lisp_Object))
{
  fun (lm->registers[0]);
  fun (lm->registers[1]);
  fun (lm->err);
}

static void
step_eval (struct lispmachine *lm)
{
  Lisp_Object env = lm->meval.env;
  Lisp_Object form = lm->meval.exp;

  Lisp_Object inlined;

  if (try_inline_eval (env, form, &inlined))
    {
      goto_return (lm, inlined);
      return;
    }

  // if not inline evalled, this is an unquoted cons

  Lisp_Object head = f_car (form);
  Lisp_Object tail = f_cdr (form);
  if (symbolp (head))
    {
      switch ((enum special_form)unbox_symbol (head)->special)
        {
        case SF_NONE:
          eval_application (lm, env, form);
          break;
        case SF_QUOTE:
          goto_return (lm, f_car (tail));
          break;
        case SF_IF:
          // (car tail) -> if cond
          // (cdr tail) -> if body (then + else)
          if (try_inline_eval (env, f_car (tail), &inlined))
            return_if (lm, env, inlined, f_cdr (tail));
          else
            {
              stack_push_if (env, f_cdr (tail));
              goto_eval (lm, env, f_car (tail));
            }
          break;
        case SF_PROGN:
          // TODO optimize: if first is atom, do not push frame
          eval_body (lm, env, tail);
          break;
        case SF_LAMBDA:
          eval_lambda (lm, env, tail);
          break;
        case SF_DEFINE:
          // (car tail) -> sym to define
          // (car (cdr tail)) -> value
          check_type (f_car (tail), LISP_SYMB);
          if (try_inline_eval (env, f_car (f_cdr (tail)), &inlined))
            return_define (lm, env, f_car (tail), inlined);
          else
            {
              stack_push_define (env, f_car (tail));
              goto_eval (lm, env, f_car (f_cdr (tail)));
            }
          break;
        case SF_SETQ:
          // (car tail) -> sym to set
          // (car (cdr tail)) -> definition
          check_type (f_car (tail), LISP_SYMB);
          if (try_inline_eval (env, f_car (f_cdr (tail)), &inlined))
            return_setq (lm, env, f_car (tail), inlined);
          else
            {
              stack_push_setq (env, f_car (tail));
              goto_eval (lm, env, f_car (f_cdr (tail)));
            }
          break;
        case SF_UNWIND_PROTECT:
          stack_push_unwindprotect (env, f_cdr (tail));
          goto_eval (lm, env, f_car (tail));
          break;
        case SF_CONDITION_CASE:
          check_type (f_car (tail), LISP_SYMB);
          stack_push_conditioncase (env, f_car (tail), f_cdr (f_cdr (tail)));
          goto_eval (lm, env, f_car (f_cdr (tail)));
          break;
        }
    }
  else
    eval_application (lm, env, form);
}

static void
step_apply (struct lispmachine *lm)
{
  // only place where we increase reductions
  lm->reductions++;

  Lisp_Object fun = lm->mapply.proc;
  Lisp_Object args = lm->mapply.args;

  if (!lambdap (fun) && !subrp (fun))
    invalidfunc_error (fun);

  struct lisp_callable_header *header = unbox_pointer (fun);
  int minargs = header->minargs;
  int maxargs = header->maxargs;
  int nargs = unbox_int (f_length (args));

  if (nargs < minargs || nargs > maxargs)
    funcargs_error (minargs, maxargs, nargs);

  int arity = maxargs != MANY ? maxargs : nargs;

  Lisp_Object *argvals = alloca (arity * sizeof (Lisp_Object));
  Lisp_Object tail = args;

  for (int i = 0; i < arity; i++)
    {
      if (i >= nargs)
        {
          // pad with nils
          argvals[i] = q_nil;
          continue;
        }

      argvals[i] = f_car (tail);
      tail = f_cdr (tail);
    }

  if (lambdap (fun))
    {
      Lisp_Lambda *ulambda = unbox_lambda (fun);
      Lisp_Object env = env_new (ulambda->env);
      for (int i = 0; i < minargs; i++)
        {
          Lisp_Object argsym = ulambda->args[i];
          Lisp_Object argval = argvals[i];
          env_define (env, argsym, argval);
        }

      if (maxargs == MANY)
        {
          // pack each argument after minargs in a lisp list
          Lisp_Object restarg = q_nil;
          for (int i = nargs - 1; i >= minargs; i--)
            restarg = f_cons (argvals[i], restarg);

          // bind the rest arg in the lambda last symbol
          Lisp_Object argsym = ulambda->args[ulambda->nargs - 1];
          env_define (env, argsym, restarg);
        }

      eval_body (lm, env, ulambda->form);
      return;
    }
  // fun is subr

  if (eq (fun, fq_apply))
    {
      // dirty trick: we hook the "apply" lisp function here because
      // it needs being integrated in the machine.
      goto_apply (lm, f_car (args), f_car (f_cdr (args)));
      return;
    }
  else if (eq (fun, fq_eval_noexpand))
    {
      // same trick for eval-noexpand
      goto_eval (lm, l_globalenv, f_car (args));
      return;
    }

  // fun is normal subr
  Lisp_Object res = call_subr (unbox_subr (fun), maxargs, arity, argvals);
  goto_return (lm, res);
}

static int
step_return (struct lispmachine *lm)
{
  struct stackframe frame = stack_pop ();
  Lisp_Object env = frame.env;
  Lisp_Object val = lm->mreturn.val;

  switch (frame.kind)
    {
    case F_HALT:
      return LMEND;
    case F_SEQ:
      eval_body (lm, env, frame.seq.rest);
      break;
    case F_ARGS:
      return_args (lm, env, val, frame.args.argl, frame.args.unev);
      break;
    case F_IF:
      return_if (lm, env, val, frame.if_.body);
      break;
    case F_DEFINE:
      return_define (lm, env, frame.define.symbol, val);
      break;
    case F_SETQ:
      return_setq (lm, env, frame.setq.symbol, val);
      break;
    case F_CONDITION_CASE:
      // this is the case without errors, just go ahead
      goto_return (lm, val);
      break;
    case F_UNWIND_PROTECT:
      stack_push_restore (frame.env, val);
      eval_body (lm, frame.env, frame.unwindprotect.cleanups);
      break;
    case F_RERAISE:
      xsignal_err (frame.reraise.err);
      break;
    case F_RESTORE:
      goto_return (lm, frame.restore.val);
      break;
    }
  return LMCONTINUE;
}

static Lisp_Object
call_subr (Lisp_Subr *usubr, int maxargs, int arity, Lisp_Object *argvals)
{
  if (maxargs == MANY)
    return usubr->function.f999 (arity, argvals);

  switch (arity)
    {
    case 0:
      return usubr->function.f0 ();
      break;
    case 1:
      return usubr->function.f1 (argvals[0]);
      break;
    case 2:
      return usubr->function.f2 (argvals[0], argvals[1]);
      break;
    case 3:
      return usubr->function.f3 (argvals[0], argvals[1], argvals[2]);
      break;
    case 4:
      return usubr->function.f4 (argvals[0], argvals[1], argvals[2],
                                 argvals[3]);
      break;
    case 5:
      return usubr->function.f5 (argvals[0], argvals[1], argvals[2],
                                 argvals[3], argvals[4]);
      break;
    case 6:
      return usubr->function.f6 (argvals[0], argvals[1], argvals[2],
                                 argvals[3], argvals[4], argvals[5]);
      break;
    case 7:
      return usubr->function.f7 (argvals[0], argvals[1], argvals[2],
                                 argvals[3], argvals[4], argvals[5],
                                 argvals[6]);
    case 8:
      return usubr->function.f8 (argvals[0], argvals[1], argvals[2],
                                 argvals[3], argvals[4], argvals[5],
                                 argvals[6], argvals[7]);
    default:
      internal_error ("Illegal state - Invoking subr with %d>8 args", arity);
    }
}

static int
try_inline_eval (Lisp_Object env, Lisp_Object form, Lisp_Object *out)
{
  if (symbolp (form))
    {
      // symbol: inline eval simply finding binding
      *out = find_binding (env, form);
      return 1;
    }
  if (consp (form))
    {
      // cons: only (quote subform) can be inlined
      if (eq (f_car (form), q_quote))
        {
          *out = f_car (f_cdr (form));
          return 1;
        }
      return 0;
    }
  // all other types are self-evaluating
  *out = form;
  return 1;
}

static Lisp_Object
find_binding (Lisp_Object env, Lisp_Object symbol)
{
  if (eq (symbol, q_t) || nil (symbol) || unbound (symbol))
    return symbol;

  Lisp_Object val;

  Lisp_Symbol *usymbol = unbox_symbol (symbol);
  if (!usymbol->localbound)
    {
      // just a petty "optimization" with a trick. if the symbol is not
      // local bound, i.e. it has only purely global binding, we don't
      // bother looking up the symbol in env: we return its global
      // value, that by definition is stored in the symbol itself
      val = usymbol->value;
      if (unbound (val))
        unbound_error (symbol);
      return val;
    }

  // lookup symbol in env
  val = env_lookup (env, symbol);
  if (!unbound (val))
    return val;

  // return value attached to symbol (global)
  val = unbox_symbol (symbol)->value;
  if (!unbound (val))
    return val;

  unbound_error (symbol);
}

static void
eval_application (struct lispmachine *lm, Lisp_Object env, Lisp_Object form)
{
  Lisp_Object inlined;

  if (try_inline_eval (env, f_car (form), &inlined))
    {
      return_args (lm, env, inlined, q_nil, f_cdr(form));
      return;
    }

  stack_push_args (env, q_nil, f_cdr (form));
  goto_eval (lm, env, f_car (form));
}

static void
eval_body (struct lispmachine *lm, Lisp_Object env, Lisp_Object tail)
{
  if (nil (tail))
    {
      goto_return (lm, q_nil);
      return;
    }

  if (!nil (f_cdr (tail)))
    stack_push_seq (env, f_cdr (tail));

  goto_eval (lm, env, f_car (tail));
}

static void
eval_lambda (struct lispmachine *lm, Lisp_Object env, Lisp_Object form)
{
  Lisp_Object args = f_car (form);
  Lisp_Object body = f_cdr (form);

  size_t argsize;
  int minargs = 0;
  int maxargs;
  Lisp_Object tail = args;
  while (consp (tail))
    {
      minargs++;
      tail = f_cdr (tail);
    }

  if (nil (tail))
    {
      // no &rest arguments
      argsize = maxargs = minargs;
    }
  else if (symbolp (tail))
    {
      // &rest arguments represented by symbol
      maxargs = MANY;
      argsize = minargs + 1;
    }
  else
    type_error (tail, LISP_SYMB);

  Lisp_Object *argv = alloca (argsize * sizeof (Lisp_Object));

  Lisp_Object argtail = args;
  for (int i = 0; i < minargs; i++)
    {
      Lisp_Object argsym = f_car (argtail);
      check_type (argsym, LISP_SYMB);

      // dirty trick to optimize lookups of purely global symbols
      unbox_symbol (argsym)->localbound = 1;

      argv[i] = argsym;
      argtail = f_cdr (argtail);
    }
  if (maxargs == MANY)
    {
      // fill last slot with rest argument
      unbox_symbol (tail)->localbound = 1;
      argv[minargs] = tail;
    }

  Lisp_Object lambda
      = make_lambda (minargs, maxargs, argsize, env, argv, body);
  goto_return (lm, lambda);
}

NORETURN void
xsignal_err (Lisp_Object err)
{
  if (currentlm == NULL)
    {
      internal_error ("no lisp machine active");
      exit (1);
    }
  currentlm->err = err;
  longjmp (currentlm->jmp, 1);
}

// maybe this is also useful somewhere else?
static inline Lisp_Object
make_error (Lisp_Object symbol, Lisp_Object data, Lisp_Object backtrace)
{
  return f_cons (symbol, f_cons (backtrace, data));
}

NORETURN void
xsignal (Lisp_Object symbol, Lisp_Object data)
{
  Lisp_Object backtrace = stack_backtrace (1);
  Lisp_Object error = make_error (symbol, data, backtrace);
  xsignal_err (error);
}

static void
return_if (struct lispmachine *lm, Lisp_Object env, Lisp_Object cond,
           Lisp_Object body)
{
  if (nil (cond))
    eval_body (lm, env, f_cdr (body));
  else
    goto_eval (lm, env, f_car (body));
}

static void
return_define (struct lispmachine *lm, Lisp_Object env, Lisp_Object symbol,
               Lisp_Object value)
{
  if (eq (env, l_globalenv))
    // top level: define in global
    unbox_symbol (symbol)->value = value;
  else
    env_define (env, symbol, value);

  goto_return (lm, value);
}

static void
return_setq (struct lispmachine *lm, Lisp_Object env, Lisp_Object symbol,
             Lisp_Object value)
{
  Lisp_Object binding = env_lookup_cell (env, symbol);
  if (!nil (binding))
    // local
    f_setcdr (binding, value);
  else if (!unbound (unbox_symbol (symbol)->value))
    // global
    unbox_symbol (symbol)->value = value;
  else
    unbound_error (symbol);

  goto_return (lm, value);
}

static void
return_args (struct lispmachine *lm, Lisp_Object env, Lisp_Object value,
             Lisp_Object argl, Lisp_Object unev)
{
  Lisp_Object arglreversed = f_cons (value, argl);

  Lisp_Object inlined;
  while (!nil (unev) && try_inline_eval (env, f_car (unev), &inlined))
    {
      // inlined arguments
      arglreversed = f_cons (inlined, arglreversed);
      unev = f_cdr (unev);
    }

  if (nil (unev))
    {
      // every arg is evalled, apply

      // reverse the argl (we appended to head to grow it)
      // TODO this could be a useful builtin func for reverse list
      Lisp_Object argsorted = q_nil;
      Lisp_Object tail = arglreversed;
      while (!nil (tail))
        {
          argsorted = f_cons (f_car (tail), argsorted);
          tail = f_cdr (tail);
        }
      goto_apply (lm, f_car (argsorted), f_cdr (argsorted));
    }
  else
    {
      // still args to eval
      stack_push_args (env, arglreversed, f_cdr (unev));
      goto_eval (lm, env, f_car (unev));
    }
}
