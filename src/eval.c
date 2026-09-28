#include <alloca.h>
#include <setjmp.h>
#include <stdio.h>

#include "alloc.h"
#include "debug.h"
#include "env.h"
#include "eval.h"
#include "lisp.h"
#include "obarray.h"

struct stackframe stack[STACKSIZE];
int stackdepth = 0;

void
stack_push (struct stackframe sf)
{
  if (stackdepth >= STACKSIZE)
    {
      // TODO err
      fprintf (stderr, "stack size exceeded: %d\n", STACKSIZE);
      exit (9);
    }

  stack[stackdepth++] = sf;
}

struct stackframe
stack_pop ()
{
  if (stackdepth <= 0)
    {
      // TODO err
      fprintf (stderr, "already at beginning of stack\n");
      exit (9);
    }

  return stack[--stackdepth];
}

struct stackframe
stack_current ()
{
  return stack[stackdepth - 1];
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
      // TODO
      exit (666);
    }

  debug_print_form (res);
  debug_printf ("\n");
  return res;
}

Lisp_Object
eval_symbol (Lisp_Object env, Lisp_Object symbol)
{
  if (eq (symbol, q_t))
    return q_t;
  if (eq (symbol, q_nil))
    return q_nil;
  if (eq (symbol, q_unbound))
    return q_unbound;

  Lisp_Object val;
  Lisp_Symbol *usymbol = unbox_symbol (symbol);

  val = usymbol->value;
  if (!eq (val, q_unbound))
    {
      // value already in the symbol
      return val;
    }

  Lisp_Object lookedup;
  // lookup symbol in env
  lookedup = env_lookup (env, symbol);
  if (eq (lookedup, q_unbound) || eq (lookedup, q_nil))
    {
      // lookup symbol in obarray
      lookedup = obarray_lookup (v_obarray, symbol);
      if (type_of (lookedup) != LISP_SYMB)
        {
          // todo err
          fprintf (stderr, "unbound symbol: %s\n",
                   unbox_string (unbox_symbol (symbol)->name)->data);
          exit (13);
        }
    }

  // store val in symbol for faster lookups. TODO is this dangerous?
  val = unbox_symbol (lookedup)->value;
  return val;
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

  Lisp_Object fun = eval_symbol (env, funsym);
  if (eq (fun, q_unbound))
    {
      // TODO err unbound function
      fprintf (stderr, "unbound function: '%s'\n",
               unbox_string (unbox_symbol (funsym)->name)->data);
      exit (2);
    }

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
      // TODO error
      fprintf (stderr, "illegal function type: %s\n",
               type_name (type_of (fun)));
      printf ("symbol ");
      print_form (funsym);
      printf (" -> ");
      print_form (fun);
      printf ("\n");
      exit (12);
    }

  int nargs = unbox_int (f_length (funargs));

  if (nargs < minargs || nargs > maxargs)
    {
      // TODO error
      fprintf (stderr,
               "wrong n of arguments: got %d, expected min %d, max %d\n",
               nargs, minargs, maxargs);
      return q_nil;
    }

  stack_push ((struct stackframe){ .fname = fname, .env = env });

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
          // TODO ugly if, this code sucks
          fprintf (stderr, "lambda cannot have unevalled args\n");
          exit (23);
        }
      // TODO ugly return in a switch that should decide arity!
      result = call_unevalled_subr (subr, funargs);
      stack_pop_free ();
      return result;
    default:
      arity = maxargs;
    }

  // FIXME: argvals are never collected by gc...
  Lisp_Object *argvals = alloca (arity * sizeof (Lisp_Object));
  Lisp_Object argtail = funargs;

  for (int i = 0; i < arity; i++)
    {
      if (i >= nargs)
        {
          // pad with nils
          argvals[i] = q_nil;
          continue;
        }

      argvals[i] = eval (env, f_car (argtail));
      argtail = f_cdr (argtail);
    }

  if (type_of (fun) == LISP_SUBR)
    result = call_subr (subr, maxargs, arity, argvals);
  else // is lambda
    result = call_lambda (env, lambda, argvals);

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
      // TODO error
      fprintf (stderr, "max explicit SUBR arguments is 8\n");
      exit (321);
    }
}

Lisp_Object
call_lambda (Lisp_Object env, Lisp_Lambda *ulambda, Lisp_Object *argvals)
{
  // create a new environment binding lambda arg symbols to actual values
  Lisp_Object lambdaenv = env;
  for (int i = 0; i < ulambda->maxargs; i++)
    {
      Lisp_Object argsym = make_symbol (unbox_symbol (ulambda->args[i])->name);
      unbox_symbol (argsym)->value = argvals[i];
      lambdaenv = env_new (lambdaenv, argsym);
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
  Lisp_Object argform, arg, argval;

  while (!eq (argstail, q_nil))
    {
      argform = f_car (argstail);
      arg = f_car (argform);
      if (type_of (arg) != LISP_SYMB)
        {
          // TODO
          fprintf (stderr, "malformed let, trying to assing to non symbol\n");
          exit (31);
        }
      argval = eval (letenv, f_car (f_cdr (argform)));
      unbox_symbol (arg)->value = argval;
      letenv = env_new (letenv, arg);
      argstail = f_cdr (argstail);
    }

  stack_push ((struct stackframe){ .fname = "let", .env = letenv });
  Lisp_Object res = progn (letenv, body);
  stack_pop_free ();

  return res;
}

Lisp_Object
define (Lisp_Object env, Lisp_Object form)
{
  Lisp_Object var = f_car (form);
  // type safe must be symbol
  Lisp_Object value = eval (env, f_car (f_cdr (form)));
  unbox_symbol (var)->value = value;

  Lisp_Object newenv = env_new (env, var);

  // TODO this is not thread safe :(
  // TODO this seems very wrong

  // set new env in the parent stack (the current is the one in which
  // "define" is evalled and will die afterwards
  stack_parent_set_env (newenv);

  return value;
}

// maybe this is also useful somewhere else?
static Lisp_Object
make_error (Lisp_Object symbol, Lisp_Object data, Lisp_Object backtrace)
{
  return f_cons (symbol, f_cons (backtrace, data));
}

void
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
