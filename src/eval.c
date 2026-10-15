#include "eval.h"
#include "alloc.h"
#include "env.h"
#include "error.h"
#include "lisp.h"
#include "print.h"
#include "stack.h"
#include <alloca.h>
#include <setjmp.h>
#include <stdio.h>

// TODO this number is completely random
#define HANDLERSIZE 512
struct handler
{
  jmp_buf jmp;
  int stackind;
  // gc inhibit level to restore after a longjmp, e.g. from an error
  // raised during macro expansion
  int gcinhibit;
  Lisp_Object error;
};

static struct handler handlers[HANDLERSIZE];
static int handlerdepth = 0;

struct handler *
push_handler (int stackind)
{
  if (handlerdepth >= HANDLERSIZE)
    xsignal (q_error_maxhandlerdepth, box_int (HANDLERSIZE));

  struct handler *h = &handlers[handlerdepth++];
  h->error = q_nil;
  h->stackind = stackind;
  h->gcinhibit = gc_inhibit_level ();
  return h;
}

struct handler *
current_handler ()
{
  return &handlers[handlerdepth - 1];
}

struct handler *
pop_handler ()
{
  return &handlers[--handlerdepth];
}

void
handlers_walk (void (*fun) (Lisp_Object))
{
  for (int i = handlerdepth - 1; i >= 0; i--)
    fun (handlers[i].error);
}

Lisp_Object
eval_toplevel (Lisp_Object env, Lisp_Object form)
{
  // the expander keeps partial results in C locals while running macro
  // code, which can trigger a gc: no gc during expansion
  gc_inhibit ();
  form = macroexpand_all (form);
  gc_allow ();
  return eval (env, form);
}

Lisp_Object
eval (Lisp_Object env, Lisp_Object form)
{
  Lisp_Object res;
  debug_printf ("EVAL ");
  debug_print_form (form);
  debug_printf (" --> ");
  switch (type_of (form))
    {
    case LISP_INTG:
    case LISP_CHAR:
    case LISP_FLOT:
    case LISP_STRG:
    case LISP_VECT:
    case LISP_SUBR:
    case LISP_LMBD:
    case LISP_PORT:
      // eval to themself
      res = form;
      break;
    case LISP_SYMB:
      res = eval_symbol (env, form);
      break;
    case LISP_CONS:
      res = call_function (env, form);
      break;
    }

  debug_print_form (res);
  debug_printf ("\n");
  return res;
}

Lisp_Object
eval_symbol (Lisp_Object env, Lisp_Object symbol)
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

static Lisp_Object
apply_internal (Lisp_Object fun, int nargs, Lisp_Object *args)
{
  if (!lambdap (fun) && !subrp (fun))
    invalidfunc_error (fun);

  struct lisp_callable_header *header = unbox_pointer (fun);

  int minargs = header->minargs;
  int maxargs = header->maxargs;

  if (maxargs == UNEVALLED)
    // special form not appliable
    invalidfunc_error (fun);

  if (nargs < minargs || nargs > maxargs)
    funcargs_error (minargs, maxargs, nargs);

  int arity = maxargs != MANY ? maxargs : nargs;

  Lisp_Object *argvals;
  if (arity > nargs)
    {
      // pad with nil
      argvals = alloca (arity * sizeof (Lisp_Object));
      for (int i = 0; i < nargs; i++)
        argvals[i] = args[i];
      for (int i = nargs; i < arity; i++)
        argvals[i] = q_nil;
    }
  else
    argvals = args;

  if (subrp (fun))
    return call_subr (unbox_subr (fun), maxargs, arity, argvals);
  else // is lambda
    return call_lambda (unbox_lambda (fun), nargs, argvals);
}

Lisp_Object
apply (Lisp_Object env, Lisp_Object fun, Lisp_Object args)
{
  int nargs = unbox_int (f_length (args));
  Lisp_Object *aargs = alloca (nargs * sizeof (Lisp_Object));

  Lisp_Object tail = args;
  int i = 0;
  while (!nil (tail))
    {
      aargs[i] = f_car (tail);
      tail = f_cdr (tail);
      i++;
    }

  stack_push ((struct stackframe){
      .fname = "anonymous",
      .env = env,
      .form = q_nil,
      .argvals = aargs,
      .nargs = nargs,
      .keepalive = fun,
  });
  Lisp_Object res = apply_internal (fun, nargs, aargs);
  stack_pop ();
  return res;
}

Lisp_Object
call_function (Lisp_Object env, Lisp_Object form)
{
  Lisp_Object funyielding = f_car (form);
  Lisp_Object funargs = f_cdr (form);
  Lisp_Object fun = eval (env, funyielding);

  const char *fname;
  if (lambdap (fun))
    fname = symbolp (funyielding) ? symbol_name (funyielding) : "anonymous";
  else if (subrp (fun))
    fname = unbox_subr (fun)->name;
  else
    invalidfunc_error (fun);

  struct lisp_callable_header *header = unbox_pointer (fun);
  int maxargs = header->maxargs;

  if (maxargs == UNEVALLED)
    {
      // this UNEVALLED in maxargs is a old dirty trick in emacs lisp
      // for handling special forms
      if (type_of (fun) == LISP_LMBD)
        // not supported YET but it would be fun!!
        // https://web.cs.wpi.edu/~jshutt/dissertation/etd-090110-124904-Shutt-Dissertation.pdf
        internal_error (
            "Lambda cannot have unevalled args; fexpr not supported");

      stack_push ((struct stackframe){
          .fname = fname,
          .env = env,
          .form = form,
          .nargs = 0,
      });

      // gc is orrible here, but here all roots are protected
      // TODO find an elegand solution to this horror
      if (gc_maybe ())
        debug_printf ("GC executed");

      // TODO ugly return in a switch that should decide arity!
      Lisp_Object result = call_unevalled_subr (unbox_subr (fun), funargs);
      stack_pop ();
      return result;
    }

  // this will traverse the list; is this correct??
  int nargs = unbox_int (f_length (funargs));

  Lisp_Object *argvals = alloca (nargs * sizeof (Lisp_Object));
  for (int i = 0; i < nargs; i++)
    argvals[i] = q_nil;

  stack_push ((struct stackframe){
      .fname = fname,
      .env = env,
      .form = form,
      .argvals = argvals,
      .nargs = nargs,
  });

  // gc is orrible here, but here all roots are protected
  // TODO find an elegand solution to this horror
  if (gc_maybe ())
    debug_printf ("GC executed");

  Lisp_Object argtail = funargs;
  for (int i = 0; i < nargs; i++)
    {
      argvals[i] = eval (env, f_car (argtail));
      argtail = f_cdr (argtail);
    }

  Lisp_Object result = apply_internal (fun, nargs, argvals);

  stack_pop ();

  return result;
}

Lisp_Object
call_unevalled_subr (Lisp_Subr *usubr, Lisp_Object form)
{
  return usubr->function.f888 (form);
}

Lisp_Object
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

Lisp_Object
call_lambda (Lisp_Lambda *ulambda, int nargs, Lisp_Object *argvals)
{
  // TODO nargs, minargs, maxargs, arity, lambda->nargs, ... what a mess, please more consistent naming!!
  int minargs = ulambda->header.minargs;
  int maxargs = ulambda->header.maxargs;
  // create a new environment binding lambda arg symbols to actual values
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

  // set lambda env in current stack for protecting from GC.
  stack_current_set_env (env);

  // recursively eval the lambda body
  return progn (env, ulambda->form);
}

Lisp_Object
progn (Lisp_Object env, Lisp_Object form)
{
  // TODO it seems that in every function i have used a different way to
  // traverse the list... awful, pls consistency!!!
  Lisp_Object val = q_nil;
  Lisp_Object tail = form;

  while (!nil (tail))
    {
      val = eval (env, f_car (tail));
      tail = f_cdr (tail);
    }

  return val;
}

Lisp_Object
let (Lisp_Object env, Lisp_Object form)
{
  Lisp_Object args = f_car (form);
  Lisp_Object argstail = args;
  Lisp_Object body = f_cdr (form);
  Lisp_Object letenv = env_new (env);
  Lisp_Object argform, argsym, argval;

  stack_push (
      (struct stackframe){ .fname = "let", .env = letenv, .form = form });

  while (!nil (argstail))
    {
      argform = f_car (argstail);
      argsym = f_car (argform);
      check_type (argsym, LISP_SYMB);

      // dirty trick to optimize lookups of purely global symbols
      unbox_symbol (argsym)->localbound = 1;

      argval = eval (letenv, f_car (f_cdr (argform)));
      env_define (letenv, argsym, argval);
      argstail = f_cdr (argstail);
    }

  Lisp_Object res = progn (letenv, body);
  stack_pop ();

  return res;
}

Lisp_Object
define (Lisp_Object env, Lisp_Object form)
{
  Lisp_Object var = f_car (form);
  check_type (var, LISP_SYMB);

  Lisp_Object value = eval (env, f_car (f_cdr (form)));

  if (eq (env, l_globalenv))
    {
      // top level: define in global
      unbox_symbol (var)->value = value;
    }
  else
    {
      unbox_symbol (var)->localbound = 1;
      env_define (env, var, value);
    }

  return value;
}

static inline Lisp_Object
expand_list (Lisp_Object form)
{
  if (nil (form))
    return macroexpand_all (form);
  return f_cons (macroexpand_all (f_car (form)), expand_list (f_cdr (form)));
}

static inline Lisp_Object
expand_define (Lisp_Object form)
{
  Lisp_Object defineform = f_cdr (form);
  Lisp_Object var = f_car (defineform);

  if (!consp (var))
    // normal form of define
    return expand_list (form);

  // shortened form for defining functions:
  // (define (f arg) (body)) instead of (define f (lambda (arg) (body)))

  Lisp_Object symbol = f_car (var);
  Lisp_Object args = f_cdr (var);
  Lisp_Object body = f_cdr (defineform);

  Lisp_Object lambdaform = expand_list (body);
  lambdaform = f_cons (args, lambdaform);
  lambdaform = f_cons (q_lambda, lambdaform);
  Lisp_Object expanded = q_nil;
  expanded = f_cons (lambdaform, expanded);
  expanded = f_cons (symbol, expanded);
  expanded = f_cons (q_define, expanded);

  return expanded;
}

static inline Lisp_Object
expand_defmacro (Lisp_Object form)
{
  Lisp_Object defmacroform = f_cdr (form);
  Lisp_Object var = f_car (defmacroform);

  check_type (var, LISP_CONS);

  Lisp_Object symbol = f_car (var);
  Lisp_Object args = f_cdr (var);
  Lisp_Object body = f_cdr (defmacroform);
  check_type (symbol, LISP_SYMB);
  check_type (body, LISP_CONS);

  Lisp_Object lambdaform = expand_list (body);
  lambdaform = f_cons (args, lambdaform);
  lambdaform = f_cons (q_lambda, lambdaform);

  // (define name (cons (quote macro) (lambda args . body)))
  Lisp_Object quoted = list2 (q_quote, q_macro);
  Lisp_Object value = list3 (q_cons, quoted, lambdaform);
  return list3 (q_define, symbol, value);
}

static inline Lisp_Object
expand_lambda (Lisp_Object form)
{
  // (lambda args . body): args are not code, expand only the body
  Lisp_Object lambdaform = f_cdr (form);
  Lisp_Object args = f_car (lambdaform);
  Lisp_Object body = f_cdr (lambdaform);

  lambdaform = expand_list (body);
  lambdaform = f_cons (args, lambdaform);
  return f_cons (q_lambda, lambdaform);
}

static inline Lisp_Object
macro_object (Lisp_Object symbol)
{
  if (!symbolp (symbol))
    return q_nil;
  // macro is only global, accessing with ->value works
  Lisp_Object macro = unbox_symbol (symbol)->value;
  // macro form is: '(macro . (lambda (args) body))
  if (consp (macro) && eq (f_car (macro), q_macro) && lambdap (f_cdr (macro)))
    return macro;
  return q_nil;
}

Lisp_Object
macroexpand_all (Lisp_Object form)
{
  if (!consp (form))
    return form;

  Lisp_Object head = f_car (form);

  if (eq (head, q_quote))
    return form;
  if (eq (head, q_define))
    return expand_define (form);
  if (eq (head, q_defmacro))
    return expand_defmacro (form);
  if (eq (head, q_lambda))
    return expand_lambda (form);
  if (!nil (macro_object (head)))
    return macroexpand_all (macroexpand_1 (form));

  // if, progn, unwind-protect and plain applications: every element
  // is code (including the head, for ((lambda ...) x))
  return expand_list (form);
}

Lisp_Object
macroexpand_1 (Lisp_Object form)
{
  if (!consp (form))
    return form;

  Lisp_Object macrosym = f_car (form);
  Lisp_Object macro = macro_object (macrosym);
  Lisp_Object macrofun = f_cdr (macro);
  if (nil (macrofun))
    return form;

  Lisp_Object macroargs = f_cdr (form);
  return apply (l_globalenv, macrofun, macroargs);
}

static inline void
run_cleanups (struct stackframe *sf)
{
  if (!consp (sf->cleanups))
    // no cleanups
    return;

  Lisp_Object cleanups = sf->cleanups;
  sf->cleanups = q_nil;
  progn (sf->env, cleanups);
}

Lisp_Object
unwind_protect (Lisp_Object env, Lisp_Object form)
{
  Lisp_Object body = f_car (form);
  Lisp_Object cleanups = f_cdr (form);

  struct stackframe *sf = stack_current_ref ();
  sf->cleanups = cleanups;

  Lisp_Object res = eval (env, body);

  // local exit: no error

  // protect res from gc during cleanups
  sf->keepalive = res;

  run_cleanups (sf);

  return res;
}

// maybe this is also useful somewhere else?
static Lisp_Object
make_error (Lisp_Object symbol, Lisp_Object data, Lisp_Object backtrace)
{
  return f_cons (symbol, f_cons (backtrace, data));
}

NORETURN void
xsignal_err (Lisp_Object err)
{
  if (handlerdepth == 0)
    {
      // no handler: no setjmp, using longjmp would have undefined behaviour
      print_error (err);
      exit (1);
    }

  struct handler *h = current_handler ();
  h->error = err;

  // unwind and cleanup need to happen here: they need the stack
  // frames from here till jump, which will die after jump
  stack_unwind (h->stackind, run_cleanups);

  longjmp (h->jmp, 1);
}

NORETURN void
xsignal (Lisp_Object symbol, Lisp_Object data)
{
  Lisp_Object backtrace = stack_backtrace (1); // 1: exclude base frame
  Lisp_Object error = make_error (symbol, data, backtrace);
  xsignal_err (error);
}

int
condition_case_0 (Lisp_Object (*fun) (), Lisp_Object *out)
{
  struct handler *h = push_handler (stack_depth_current ());

  if (setjmp (h->jmp))
    {
      *out = h->error;
      gc_inhibit_set_level (h->gcinhibit);
      pop_handler ();
      return 0;
    }
  else
    {
      *out = fun ();
      pop_handler ();
      return 1;
    }
}

int
condition_case_1 (Lisp_Object (*fun) (Lisp_Object), Lisp_Object arg1,
                  Lisp_Object *out)
{
  struct handler *h = push_handler (stack_depth_current ());

  if (setjmp (h->jmp))
    {
      *out = h->error;
      gc_inhibit_set_level (h->gcinhibit);
      pop_handler ();
      return 0;
    }
  else
    {
      *out = fun (arg1);
      pop_handler ();
      return 1;
    }
}

int
condition_case_2 (Lisp_Object (*fun) (Lisp_Object, Lisp_Object),
                  Lisp_Object arg1, Lisp_Object arg2, Lisp_Object *out)
{
  struct handler *h = push_handler (stack_depth_current ());

  if (setjmp (h->jmp))
    {
      *out = h->error;
      gc_inhibit_set_level (h->gcinhibit);
      pop_handler ();
      return 0;
    }
  else
    {
      *out = fun (arg1, arg2);
      pop_handler ();
      return 1;
    }
}

int
condition_case_n (Lisp_Object (*fun) (int, Lisp_Object *), int nargs,
                  Lisp_Object *args, Lisp_Object *out)
{
  struct handler *h = push_handler (stack_depth_current ());

  if (setjmp (h->jmp))
    {
      *out = h->error;
      gc_inhibit_set_level (h->gcinhibit);
      pop_handler ();
      return 0;
    }
  else
    {
      *out = fun (nargs, args);
      pop_handler ();
      return 1;
    }
}

int
safe_eval (Lisp_Object env, Lisp_Object form, Lisp_Object *out)
{
  return condition_case_2 (eval, env, form, out);
}

Lisp_Object
condition_case (Lisp_Object env, Lisp_Object form)
{
  Lisp_Object body = f_car (form);
  Lisp_Object conds = f_cdr (form);
  Lisp_Object res;

  if (safe_eval (env, body, &res))
    return res;

  Lisp_Object err = res;

  while (!nil (conds))
    {
      Lisp_Object cond = f_car (conds);
      Lisp_Object condsym = f_car (cond);

      Lisp_Object errsym = f_car (err);

      if (eq (condsym, q_t) ||    // catch all
          eq (errsym, condsym) || // match single
          (consp (condsym)
           && !nil (f_memq (errsym, condsym)))) // match in list
        {
          Lisp_Object condbody = f_cdr (cond);
          return progn (env, condbody);
        }

      conds = f_cdr (conds);
    }

  xsignal_err (err);
}
