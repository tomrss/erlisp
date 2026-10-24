#ifndef STACK_H
#define STACK_H

#include "lisp.h"

#define STACKPAGESIZE 1024
#define STACKMAXPAGES 8
#define STACKSIZE (STACKMAXPAGES * STACKPAGESIZE)

enum stackframe_kind
{
  F_HALT = 0,
  F_SEQ,  // sequence of operations (progn/begin)
  F_ARGS, // args of fun application
  F_IF,
  F_DEFINE,
  F_SETQ,
  F_CONDITION_CASE,
  F_UNWIND_PROTECT,
  F_RERAISE, // re-raise error after a temporary stop in the stack
  F_RESTORE, // restore value
};

struct stackframe
{
  enum stackframe_kind kind;
  Lisp_Object env;
  // clang-format off

  // NOTE: this union is just for the sake of clarity in intent,
  // sacrificing some lines of code. we could have used just 2 unnamed
  // slots and interpret them differently based on kind. i think this
  // is more clear to express what we want to hold in those registers
  // without browsing more code
  union
  {
    struct { Lisp_Object rest;             } seq;
    struct { Lisp_Object argl, unev;       } args;
    struct { Lisp_Object body;             } if_;
    struct { Lisp_Object symbol;           } define;
    struct { Lisp_Object symbol;           } setq;
    struct { Lisp_Object handlers, errsym; } conditioncase;
    struct { Lisp_Object cleanups;         } unwindprotect;
    struct { Lisp_Object val;              } restore;
    struct { Lisp_Object err;              } reraise;

    // option for reading named slots regardless of kind
    Lisp_Object slots[2];
  };
  // clang-format on
};

struct stackpage
{
  size_t depth;
  struct stackpage *next;
  struct stackframe frames[STACKPAGESIZE];
};

struct stack
{
  size_t numpages;
  struct stackpage *pages;
};

struct stack *stack_init ();
void stack_push_halt (struct stack *s, Lisp_Object env);
void stack_push_seq (struct stack *s, Lisp_Object env, Lisp_Object rest);
void stack_push_args (struct stack *s, Lisp_Object env, Lisp_Object argl, Lisp_Object unev);
void stack_push_if (struct stack *s, Lisp_Object env, Lisp_Object body);
void stack_push_define (struct stack *s, Lisp_Object env, Lisp_Object symbol);
void stack_push_setq (struct stack *s, Lisp_Object env, Lisp_Object symbol);
void stack_push_conditioncase (struct stack *s, Lisp_Object env, Lisp_Object errsym,
                               Lisp_Object handlers);
void stack_push_unwindprotect (struct stack *s, Lisp_Object env, Lisp_Object cleanups);
void stack_push_restore (struct stack *s, Lisp_Object env, Lisp_Object val);
void stack_push_reraise (struct stack *s, Lisp_Object env, Lisp_Object err);
struct stackframe stack_pop (struct stack *s);
void stack_walk (struct stack *s, void (*fun) (struct stackframe));
Lisp_Object stack_backtrace (struct stack *s, int depthfrom);

#endif /* STACK_H */
