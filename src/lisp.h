#ifndef LISP_H
#define LISP_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INTBITS 64
#define TAGBITS 3
#define TAGMASK ((1LL << TAGBITS) - 1)
#define VALBITS (INTBITS - TAGBITS)
#define VALMASK (~TAGMASK)

#define LISP_NULL 0
#define MANY 999
#define UNEVALLED 888

#define ERRTYPE(expected, got)                                                \
  fprintf (stderr, "type error. expected %s, got %s", type_name (expected),   \
           type_name (got));                                                  \
  exit (123);

#define ERRUNBOUND(symbol)                                                    \
  fprintf (stderr, "unbound variable: %s",                                    \
           unbox_string (unbox_symbol (symbol)->name)->data);                 \
  exit (171);

#define NSUBR(N, fun)                                                         \
  (union lisp_subr_fun) { .f##N = fun }

#define BOOL(expr) (expr) ? q_t : q_nil

typedef uint64_t Lisp_Object;

typedef enum
{
  LISP_INTG = 0x0,
  LISP_STRG = 0x1,
  LISP_SYMB = 0x2,
  LISP_CONS = 0x3,
  LISP_VECT = 0x4,
  LISP_SUBR = 0x5,
  LISP_LMBD = 0x6,
} Lisp_Type;

typedef int64_t Lisp_Integer;
typedef struct lisp_string Lisp_String;
typedef struct lisp_symbol Lisp_Symbol;
typedef struct lisp_cons Lisp_Cons;
typedef struct lisp_vector Lisp_Vector;
typedef struct lisp_subr Lisp_Subr;
typedef struct lisp_lambda Lisp_Lambda;

typedef Lisp_Object (*lisp_subr_fun_0) (void);
typedef Lisp_Object (*lisp_subr_fun_1) (Lisp_Object arg1);
typedef Lisp_Object (*lisp_subr_fun_2) (Lisp_Object arg1, Lisp_Object arg2);
typedef Lisp_Object (*lisp_subr_fun_3) (Lisp_Object arg1, Lisp_Object arg2,
                                        Lisp_Object arg3);
typedef Lisp_Object (*lisp_subr_fun_4) (Lisp_Object arg1, Lisp_Object arg2,
                                        Lisp_Object arg3, Lisp_Object arg4);
typedef Lisp_Object (*lisp_subr_fun_5) (Lisp_Object arg1, Lisp_Object arg2,
                                        Lisp_Object arg3, Lisp_Object arg4,
                                        Lisp_Object arg5);
typedef Lisp_Object (*lisp_subr_fun_6) (Lisp_Object arg1, Lisp_Object arg2,
                                        Lisp_Object arg3, Lisp_Object arg4,
                                        Lisp_Object arg5, Lisp_Object arg6);
typedef Lisp_Object (*lisp_subr_fun_7) (Lisp_Object arg1, Lisp_Object arg2,
                                        Lisp_Object arg3, Lisp_Object arg4,
                                        Lisp_Object arg5, Lisp_Object arg6,
                                        Lisp_Object arg7);
typedef Lisp_Object (*lisp_subr_fun_8) (Lisp_Object arg1, Lisp_Object arg2,
                                        Lisp_Object arg3, Lisp_Object arg4,
                                        Lisp_Object arg5, Lisp_Object arg6,
                                        Lisp_Object arg7, Lisp_Object arg8);
typedef Lisp_Object (*lisp_subr_fun_many) (int argc, Lisp_Object *argv);

struct lisp_cons
{
  Lisp_Object car;
  Lisp_Object cdr;
};

struct lisp_string
{
  size_t size;
  const char *data;
};

struct lisp_symbol
{
  Lisp_Object name;
  Lisp_Object value;
  // next symbol in the obarray bucket, see obarray.h.  this is
  // inspired from emacs lisp
  struct lisp_symbol *next;
};

struct lisp_vector
{
  size_t size;
  // flexible array member
  Lisp_Object contents[];
};

union lisp_subr_fun
{
  lisp_subr_fun_0 f0;
  lisp_subr_fun_1 f1;
  lisp_subr_fun_2 f2;
  lisp_subr_fun_3 f3;
  lisp_subr_fun_4 f4;
  lisp_subr_fun_5 f5;
  lisp_subr_fun_6 f6;
  lisp_subr_fun_7 f7;
  lisp_subr_fun_8 f8;
  // sorry for for magic numbers 888 and 999, it's a dirty trick with
  // a macro.
  // TODO: REMOVE THIS call these fUNEVALLED and fMANY
  lisp_subr_fun_1 f888;    /* unevalled */
  lisp_subr_fun_many f999; /* many */
};

struct lisp_subr
{
  const char *name;
  union lisp_subr_fun function;
  int minargs;
  int maxargs;
};

struct lisp_lambda
{
  int minargs;
  int maxargs;
  Lisp_Object *args;
  Lisp_Object form;
};

/* Type checking */

static inline Lisp_Type
type_of (Lisp_Object v)
{
  return (Lisp_Type)(v & TAGMASK);
}

static inline int
is_type (Lisp_Object v, Lisp_Type t)
{
  return type_of (v) == t;
}

/* Conversion from/to boxed Lisp_Object to/from explicit types */

static inline void *
unbox_pointer (Lisp_Object v)
{
  return (void *)(v & VALMASK);
}

static inline Lisp_Object
box_int (Lisp_Integer i)
{
  return (uint64_t)i << TAGBITS;
}

static inline Lisp_Integer
unbox_int (Lisp_Object v)
{
  return ((int64_t)v) >> TAGBITS;
}

static inline Lisp_Object
box_string (Lisp_String *s)
{
  return ((uint64_t)s) | LISP_STRG;
}

static inline Lisp_String *
unbox_string (Lisp_Object v)
{
  return (Lisp_String *)unbox_pointer (v);
}

static inline Lisp_Object
box_symbol (Lisp_Symbol *s)
{
  return ((uint64_t)s) | LISP_SYMB;
}

static inline Lisp_Symbol *
unbox_symbol (Lisp_Object v)
{
  return (Lisp_Symbol *)unbox_pointer (v);
}

static inline Lisp_Object
box_cons (Lisp_Cons *s)
{
  return ((uint64_t)s) | LISP_CONS;
}

static inline Lisp_Cons *
unbox_cons (Lisp_Object v)
{
  return (Lisp_Cons *)unbox_pointer (v);
}

static inline Lisp_Object
box_vector (Lisp_Vector *s)
{
  return ((uint64_t)s) | LISP_VECT;
}

static inline Lisp_Vector *
unbox_vector (Lisp_Object v)
{
  return (Lisp_Vector *)unbox_pointer (v);
}

static inline Lisp_Object
box_subr (Lisp_Subr *s)
{
  return ((uint64_t)s) | LISP_SUBR;
}

static inline Lisp_Subr *
unbox_subr (Lisp_Object v)
{
  return (Lisp_Subr *)unbox_pointer (v);
}

static inline Lisp_Object
box_lambda (Lisp_Lambda *s)
{
  return ((uint64_t)s) | LISP_LMBD;
}

static inline Lisp_Lambda *
unbox_lambda (Lisp_Object v)
{
  return (Lisp_Lambda *)unbox_pointer (v);
}

// TODO not inlined
static inline const char *
type_name (Lisp_Type t)
{
  switch (t)
    {
    case LISP_INTG:
      return "INTG";
    case LISP_STRG:
      return "STRG";
    case LISP_SYMB:
      return "SYMB";
    case LISP_CONS:
      return "CONS";
    case LISP_VECT:
      return "VECT";
    case LISP_SUBR:
      return "SUBR";
    case LISP_LMBD:
      return "LMBD";
    default:
      return "UNKN";
    }
}

static inline int
eq (Lisp_Object x, Lisp_Object y)
{
  return x == y;
}

// builtins.c - builtin functions
Lisp_Object f_cons (Lisp_Object car, Lisp_Object cdr);
Lisp_Object f_setcar (Lisp_Object cons, Lisp_Object car);
Lisp_Object f_setcdr (Lisp_Object cons, Lisp_Object cdr);
Lisp_Object f_vector (Lisp_Object size);
Lisp_Object f_symbol (Lisp_Object name);
Lisp_Object f_symbol_value (Lisp_Object symbol);
Lisp_Object f_car (Lisp_Object cons);
Lisp_Object f_cdr (Lisp_Object cons);
Lisp_Object f_eq_p (Lisp_Object x, Lisp_Object y);
Lisp_Object f_equal_p (Lisp_Object key, Lisp_Object alist);
Lisp_Object f_eval (Lisp_Object form);
Lisp_Object f_assoc (Lisp_Object key, Lisp_Object alist);
Lisp_Object f_assq (Lisp_Object key, Lisp_Object alist);
Lisp_Object f_rassoc (Lisp_Object key, Lisp_Object alist);
Lisp_Object f_rassq (Lisp_Object key, Lisp_Object alist);
Lisp_Object f_string_equal_p (Lisp_Object x, Lisp_Object y);
Lisp_Object f_string_length (Lisp_Object string);
Lisp_Object f_length (Lisp_Object list);
Lisp_Object f_sum (int argc, Lisp_Object *argv);
Lisp_Object f_subtract (int argc, Lisp_Object *argv);
Lisp_Object f_multiply (int argc, Lisp_Object *argv);
Lisp_Object f_divide (int argc, Lisp_Object *argv);
Lisp_Object f_ge (Lisp_Object x, Lisp_Object y);
Lisp_Object f_geq (Lisp_Object x, Lisp_Object y);
Lisp_Object f_le (Lisp_Object x, Lisp_Object y);
Lisp_Object f_leq (Lisp_Object x, Lisp_Object y);
Lisp_Object f_progn (Lisp_Object form);
Lisp_Object f_quote (Lisp_Object form);
Lisp_Object f_let (Lisp_Object form); // plain let is useless in our implementation, it's a let*
Lisp_Object f_lambda (Lisp_Object form);
Lisp_Object f_define (Lisp_Object form);

// init.c - lisp initialization and globals
extern Lisp_Object q_nil;
extern Lisp_Object q_t;
extern Lisp_Object q_unbound;
extern Lisp_Object v_obarray;
extern Lisp_Object l_globalenv;

void init_builtins ();

#endif // LISP_H
