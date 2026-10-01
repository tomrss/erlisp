#include <alloca.h>
#include <setjmp.h>
#include <stdio.h>

#include "alloc.h"
#include "debug.h"
#include "env.h"
#include "error.h"
#include "eval.h"
#include "lisp.h"

struct stackframe stack[STACKSIZE];
int stackdepth = 0;

void
stack_push (struct stackframe sf)
{
  if (stackdepth >= STACKSIZE)
    stackoverflow_error ();

  stack[stackdepth++] = sf;
}

struct stackframe
stack_pop ()
{
  if (stackdepth <= 0)
    {
      internal_error ("Unable to pop beginning of stack");
    }

  return stack[--stackdepth];
}

struct stackframe
stack_current ()
{
  return stack[stackdepth - 1];
}

void
stack_walk (void (*fun) (struct stackframe))
{
  for (int i = stackdepth - 1; i >= 0; i--)
    fun (stack[i]);
}

void
stack_parent_set_env (Lisp_Object env)
{
  int effind = stackdepth > 1 ? stackdepth - 2 : 0;

  stack[effind].env = env;
}

void
stack_current_set_env (Lisp_Object env)
{
  // TODO unmark gc?
  // TODO cannot rememember why i wrote the TODO above ??
  stack[stackdepth - 1].env = env;
}

// TODO wtf is this function??
struct stackframe
stack_pop_free ()
{
  struct stackframe pop = stack_pop ();
  /* struct stackframe cur = stack_current (); */

  /* Lisp_Object tail = pop.env; */
  /* Lisp_Object target = cur.env; */

  // TODO we cannot really free as they are managed by gc
  /* while (!eq (tail, target) && !eq (tail, q_nil)) */
  /*   { */
  /*     free_lisp_obj (f_car (tail)); */
  /*     tail = f_cdr (tail); */
  /*   } */

  return pop;
}

// TODO this number is completely random
#define HANDLERSIZE 512
struct handler
{
  jmp_buf jmp;
  int stackind;
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
    case LISP_STRG:
    case LISP_VECT:
    case LISP_SUBR:
    case LISP_LMBD:
      // eval to themself
      res = form;
      break;
    case LISP_SYMB:
      res = eval_symbol (env, form);
      break;
    case LISP_CONS:
      res = call_function (env, form);
      break;
    default:
      internal_error (
          "This is embarassing... We forgot to implment eval for a type!!!");
    }

  debug_print_form (res);
  debug_printf ("\n");
  return res;
}

Lisp_Object
eval_symbol (Lisp_Object env, Lisp_Object symbol)
{
  if (eq (symbol, q_t) || eq (symbol, q_nil) || eq (symbol, q_unbound))
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
      if (eq (val, q_unbound))
        unbound_error (symbol);
      return val;
    }

  // lookup symbol in env
  val = env_lookup (env, symbol);
  if (!eq (val, q_unbound))
    return val;

  // return value attached to symbol (global)
  val = unbox_symbol (symbol)->value;
  if (!eq (val, q_unbound))
    return val;

  unbound_error (symbol);
}

Lisp_Object
call_function (Lisp_Object env, Lisp_Object form)
{
  // TODO refactor and rationalize this function. rather ugly code.
  Lisp_Object result;
  Lisp_Subr *subr;
  Lisp_Lambda *lambda;
  int minargs;
  int maxargs;
  const char *fname;

  Lisp_Object funsym = f_car (form);
  Lisp_Object funargs = f_cdr (form);

  // TODO this is temp: we support for now calling functions attached to a
  // symbol only.
  check_type (funsym, LISP_SYMB);

  Lisp_Object fun = eval_symbol (env, funsym);

  switch (type_of (fun))
    {
    case LISP_SUBR:
      subr = unbox_subr (fun);
      minargs = subr->minargs;
      maxargs = subr->maxargs;
      fname = subr->name;
      break;
    case LISP_LMBD:
      lambda = unbox_lambda (fun);
      minargs = lambda->minargs;
      maxargs = lambda->maxargs;
      fname = unbox_string (unbox_symbol (funsym)->name)->data;
      break;
    default:
      debug_printf ("funcsym: ");
      debug_print_form (funsym);
      debug_printf ("\nfun: ");
      debug_print_form (fun);
      debug_printf ("\n");
      invalidfunc_error (fun);
    }

  int nargs = unbox_int (f_length (funargs));

  if (nargs < minargs || nargs > maxargs)
    funcargs_error (minargs, maxargs, nargs);

  /* number of arguments the function will be called with */
  int arity;
  switch (maxargs)
    {
    case MANY:
      arity = nargs;
      break;
    case UNEVALLED:
      // this UNEVALLED in maxargs is a old dirty trick in emacs lisp.
      // what really this means is that this is not a real function ma like a
      // macro that directly manipulates lisp forms instead that data, and it
      // must be expanded rather than evaluated

      if (type_of (fun) == LISP_LMBD)
        {
          internal_error (
              "Lambda cannot have unevalled args; fexpr not supported");
          // not supported YET but it would be fun!!
          // https://web.cs.wpi.edu/~jshutt/dissertation/etd-090110-124904-Shutt-Dissertation.pdf
        }

      // the FEXPR path should PROBABLY live here? in some way?

      stack_push ((struct stackframe){
          .fname = fname, .env = env, .form = form, .nargs = 0 });

      // gc is orrible here, but here all roots are protected
      // TODO find an elegand solution to this horror
      if (gc_maybe ())
        debug_printf ("GC executed");

      // TODO ugly return in a switch that should decide arity!
      result = call_unevalled_subr (subr, funargs);
      stack_pop_free ();
      return result;
    default:
      arity = maxargs;
    }

  Lisp_Object argtail = funargs;

  Lisp_Object *argvals = alloca (arity * sizeof (Lisp_Object));
  for (int i = 0; i < arity; i++)
    argvals[i] = q_nil;

  stack_push ((struct stackframe){ .fname = fname,
                                   .env = env,
                                   .form = form,
                                   .argvals = argvals,
                                   .nargs = arity });

  // gc is orrible here, but here all roots are protected
  // TODO find an elegand solution to this horror
  if (gc_maybe ())
    debug_printf ("GC executed");

  for (int i = 0; i < arity; i++)
    {
      argvals[i] = eval (env, f_car (argtail));
      argtail = f_cdr (argtail);
    }

  if (type_of (fun) == LISP_SUBR)
    result = call_subr (subr, maxargs, arity, argvals);
  else // is lambda
    result = call_lambda (lambda, argvals);

  stack_pop_free ();

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
call_lambda (Lisp_Lambda *ulambda, Lisp_Object *argvals)
{
  // create a new environment binding lambda arg symbols to actual values
  Lisp_Object lambdaenv = ulambda->env;
  for (int i = 0; i < ulambda->maxargs; i++)
    {
      Lisp_Object argsym = ulambda->args[i];
      Lisp_Object argval = argvals[i];
      lambdaenv = env_new (lambdaenv, argsym, argval);
    }

  stack_current_set_env (lambdaenv);

  // recursively eval the lambda body
  return progn (lambdaenv, ulambda->form);
}

Lisp_Object
progn (Lisp_Object env, Lisp_Object form)
{
  // TODO it seems that in every function i have used a different way to
  // traverse the list... awful, pls consistency!!!
  Lisp_Object val = q_nil;
  Lisp_Object tail = form;

  while (!eq (tail, q_nil))
    {
      val = eval (env, f_car (tail));
      tail = f_cdr (tail);
      // reload env from the stack, as prviously evalled form could have
      // changed it!
      env = env_current ();
    }

  return val;
}

Lisp_Object
let (Lisp_Object env, Lisp_Object form)
{
  Lisp_Object args = f_car (form);
  Lisp_Object argstail = args;
  Lisp_Object body = f_cdr (form);
  Lisp_Object letenv = env;
  Lisp_Object argform, argsym, argval;

  stack_push (
      (struct stackframe){ .fname = "let", .env = letenv, .form = form });

  while (!eq (argstail, q_nil))
    {
      argform = f_car (argstail);
      argsym = f_car (argform);
      check_type (argsym, LISP_SYMB);

      // dirty trick to optimize lookups of purely global symbols
      unbox_symbol (argsym)->localbound = 1;

      argval = eval (letenv, f_car (f_cdr (argform)));
      letenv = env_new (letenv, argsym, argval);
      argstail = f_cdr (argstail);
      stack_current_set_env (letenv);
    }

  Lisp_Object res = progn (letenv, body);
  stack_pop_free ();

  return res;
}

Lisp_Object
define (Lisp_Object env, Lisp_Object form)
{
  Lisp_Object var = f_car (form);
  check_type (var, LISP_SYMB);
  Lisp_Object value = eval (env, f_car (f_cdr (form)));

  if (nil (env))
    {
      // top level: define in global
      unbox_symbol (var)->value = value;
    }
  else
    {
      unbox_symbol (var)->localbound = 1;
      Lisp_Object newenv = env_new (env, var, value);

      // TODO this is not thread safe :(
      // TODO this seems very wrong

      // set new env in the parent stack (the current is the one in which
      // "define" is evalled and will die afterwards
      stack_parent_set_env (newenv);
    }

  return value;
}

// maybe this is also useful somewhere else?
static Lisp_Object
make_error (Lisp_Object symbol, Lisp_Object data, Lisp_Object backtrace)
{
  return f_cons (symbol, f_cons (backtrace, data));
}

NORETURN void
xsignal (Lisp_Object symbol, Lisp_Object data)
{
  if (handlerdepth == 0)
    {
      // no handler: no setjmp, using longjmp would have undefined behaviour
      print_error (make_error (symbol, data, q_nil));
      exit (1);
    }

  struct handler *h = current_handler ();

  Lisp_Object backtrace = q_nil;
  Lisp_Object tail = q_nil;

  // unwind the stack
  while (stackdepth > h->stackind)
    {
      struct stackframe sf = stack_pop ();
      Lisp_Object cell = f_cons (make_string (sf.fname), q_nil);

      if (eq (backtrace, q_nil))
        backtrace = cell;
      else
        f_setcdr (tail, cell);

      tail = cell;
    }

  Lisp_Object err = make_error (symbol, data, backtrace);
  h->error = err;

  longjmp (h->jmp, 1);
}

int
condition_case_0 (Lisp_Object (*fun) (), Lisp_Object *out)
{
  struct handler *h = push_handler (stackdepth);

  if (setjmp (h->jmp))
    {
      *out = h->error;
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
  struct handler *h = push_handler (stackdepth);

  if (setjmp (h->jmp))
    {
      *out = h->error;
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
  struct handler *h = push_handler (stackdepth);

  if (setjmp (h->jmp))
    {
      *out = h->error;
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
  struct handler *h = push_handler (stackdepth);

  if (setjmp (h->jmp))
    {
      *out = h->error;
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
