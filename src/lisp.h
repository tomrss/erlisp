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
#define IMMDSUBTAGBITS 3
#define IMMDSUBTAGMASK ((1LL << IMMDSUBTAGBITS) - 1)
#define IMMDPAYLOADSHIFT (TAGBITS + IMMDSUBTAGBITS)
// immediate Lisp_Type values are IMMDTYPEBASE + subtag
#define IMMDTYPEBASE (1 << TAGBITS)
#define FLOTSHIFT 32

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

#define BOOL(expr) ((expr) ? q_t : q_nil)

#ifdef __GNUC__
#define UNUSED __attribute__ ((__unused__))
#define NORETURN __attribute__ ((__noreturn__))
#else /* __GNUC__ */
#define UNUSED
#endif /* __GNUC__ */

typedef uint64_t Lisp_Object;

typedef enum
{
  LISP_TAG_INTG = 0x0,
  LISP_TAG_STRG = 0x1,
  LISP_TAG_SYMB = 0x2,
  LISP_TAG_CONS = 0x3,
  LISP_TAG_SUBR = 0x4,
  LISP_TAG_LMBD = 0x5,
  LISP_TAG_IMMD = 0x6,
  LISP_TAG_CPLX = 0x7,
} Lisp_Tag;

typedef enum
{
  // xx0: only the lowest bit counts, the other two belong to the float
  LISP_TAG_FLOT = 0x0,
  LISP_TAG_CHAR = 0x1,
} Lisp_Immd_Subtag;

typedef enum
{
  // direct tags
  LISP_INTG = LISP_TAG_INTG,
  LISP_STRG = LISP_TAG_STRG,
  LISP_SYMB = LISP_TAG_SYMB,
  LISP_CONS = LISP_TAG_CONS,
  LISP_SUBR = LISP_TAG_SUBR,
  LISP_LMBD = LISP_TAG_LMBD,
  // immediate types
  LISP_CHAR = IMMDTYPEBASE + LISP_TAG_CHAR,
  LISP_FLOT = IMMDTYPEBASE + LISP_TAG_FLOT,
  // complex types
  LISP_VECT = IMMDTYPEBASE + (1 << IMMDSUBTAGBITS),
} Lisp_Type;

// TODO use GMP for arbitrary big integers
typedef int64_t Lisp_Integer;
// TODO box a double in some way (reduce the exponent?)
typedef float Lisp_Float; 
// TODO use 32 bit integer and handle utf8 and whathever
typedef unsigned char Lisp_Char;
typedef struct lisp_string Lisp_String;
typedef struct lisp_symbol Lisp_Symbol;
typedef struct lisp_cons Lisp_Cons;
typedef struct lisp_subr Lisp_Subr;
typedef struct lisp_lambda Lisp_Lambda;
typedef struct lisp_vector Lisp_Vector;

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
  char data[];
};

struct lisp_symbol
{
  // whether symbol has let or lambda local bindings; if false, it is global
  char localbound;
  Lisp_Object name;
  // this value is ONLY for global scope. the couple localbound-value
  // gives us a simple way to avoid keeping a separate hash table for
  // the globals (maybe it would be a better idea?)
  Lisp_Object value;
  // next symbol in the obarray bucket, see obarray.h.  this is
  // inspired from emacs lisp
  struct lisp_symbol *next;
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
  Lisp_Object env;
  Lisp_Object form;
  Lisp_Object args[];
};

// complex types

struct lisp_cplx_header
{
  Lisp_Type type;
};

struct lisp_vector
{
  struct lisp_cplx_header header;
  size_t size;
  Lisp_Object contents[];
};

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
  return ((uint64_t)s) | LISP_TAG_CPLX;
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

static inline Lisp_Object
box_char (Lisp_Char c)
{
  return ((uint64_t)c << IMMDPAYLOADSHIFT)
         | ((uint64_t)LISP_TAG_CHAR << TAGBITS) | LISP_TAG_IMMD;
}

static inline Lisp_Char
unbox_char (Lisp_Object v)
{
  return (Lisp_Char)(v >> IMMDPAYLOADSHIFT);
}

static inline Lisp_Object
box_float (Lisp_Float f)
{
  uint32_t bits;
  memcpy (&bits, &f, sizeof bits);
  return ((uint64_t)bits << FLOTSHIFT) | ((uint64_t)LISP_TAG_FLOT << TAGBITS)
         | LISP_TAG_IMMD;
}

static inline Lisp_Float
unbox_float (Lisp_Object v)
{
  uint32_t bits = (uint32_t)(v >> FLOTSHIFT);
  Lisp_Float f;
  memcpy (&f, &bits, sizeof f);
  return f;
}


/* Type checking */

static inline Lisp_Tag
tag_of (Lisp_Object v)
{
  return (Lisp_Tag)(v & TAGMASK);
}

static inline Lisp_Type
immediate_type_of (Lisp_Object v)
{
  Lisp_Immd_Subtag subtag
      = (Lisp_Immd_Subtag)((v >> TAGBITS) & IMMDSUBTAGMASK);
  if ((subtag & 1) == 0)
    return LISP_FLOT;
  return (Lisp_Type)(IMMDTYPEBASE + subtag);
}

static inline Lisp_Type
complex_type_of (Lisp_Object v)
{
  return ((struct lisp_cplx_header *)(unbox_pointer (v)))->type;
}

static inline Lisp_Type
type_of (Lisp_Object v)
{
  Lisp_Tag tag = tag_of (v);
  if (tag < LISP_TAG_IMMD)
    return (Lisp_Type)tag;
  if (tag == LISP_TAG_IMMD)
    return immediate_type_of (v);
  return complex_type_of (v);
}

static inline int
is_type (Lisp_Object v, Lisp_Type t)
{
  return type_of (v) == t;
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
    case LISP_FLOT:
      return "FLOT";
    case LISP_CHAR:
      return "CHAR";
    }
  return "UNKN";
}

// builtins.c - builtin functions and globals

Lisp_Object f_cons (Lisp_Object car, Lisp_Object cdr);
Lisp_Object f_setcar (Lisp_Object cons, Lisp_Object car);
Lisp_Object f_setcdr (Lisp_Object cons, Lisp_Object cdr);
Lisp_Object f_vector (Lisp_Object size);
Lisp_Object f_symbol (Lisp_Object name);
Lisp_Object f_symbol_value (Lisp_Object symbol);
Lisp_Object f_car (Lisp_Object cons);
Lisp_Object f_cdr (Lisp_Object cons);
Lisp_Object f_cadr (Lisp_Object cons);
Lisp_Object f_cddr (Lisp_Object cons);
Lisp_Object f_eq_p (Lisp_Object x, Lisp_Object y);
Lisp_Object f_equal_p (Lisp_Object key, Lisp_Object alist);
Lisp_Object f_number_p (Lisp_Object o);
Lisp_Object f_list_p (Lisp_Object o);
Lisp_Object f_cons_p (Lisp_Object o);
Lisp_Object f_symbol_p (Lisp_Object o);
Lisp_Object f_string_p (Lisp_Object o);
Lisp_Object f_vector_p (Lisp_Object o);
Lisp_Object f_char_p (Lisp_Object o);
Lisp_Object f_null_p (Lisp_Object o);
Lisp_Object f_setq (Lisp_Object form);
Lisp_Object f_eval (Lisp_Object form);
Lisp_Object f_assoc (Lisp_Object key, Lisp_Object alist);
Lisp_Object f_assq (Lisp_Object key, Lisp_Object alist);
Lisp_Object f_rassoc (Lisp_Object key, Lisp_Object alist);
Lisp_Object f_rassq (Lisp_Object key, Lisp_Object alist);
Lisp_Object f_string_equal_p (Lisp_Object x, Lisp_Object y);
Lisp_Object f_string_length (Lisp_Object string);
Lisp_Object f_number_to_string (Lisp_Object number);
Lisp_Object f_string_to_number (Lisp_Object string);
Lisp_Object f_concat (int argc, Lisp_Object *argv);
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
Lisp_Object f_let (Lisp_Object form); // it's really a let*
Lisp_Object f_and (Lisp_Object form);
Lisp_Object f_or (Lisp_Object form);
Lisp_Object f_if (Lisp_Object form);
Lisp_Object f_when (Lisp_Object form);
Lisp_Object f_unless (Lisp_Object form);
Lisp_Object f_cond (Lisp_Object form);
Lisp_Object f_lambda (Lisp_Object form);
Lisp_Object f_define (Lisp_Object form);
Lisp_Object f_format (int argc, Lisp_Object *argv);
Lisp_Object f_load (Lisp_Object path);
Lisp_Object f_read (Lisp_Object string);
NORETURN Lisp_Object f_signal (Lisp_Object symbol, Lisp_Object data);
Lisp_Object f_error_symbol (Lisp_Object err);
Lisp_Object f_error_backtrace (Lisp_Object err);
Lisp_Object f_error_data (Lisp_Object err);
Lisp_Object f_intern (Lisp_Object name);
Lisp_Object f_gc (Lisp_Object printmemstats);
Lisp_Object f_memstats ();
Lisp_Object f_memdump ();

extern Lisp_Object q_nil;
extern Lisp_Object q_t;
extern Lisp_Object q_unbound;
extern Lisp_Object q_error;
extern Lisp_Object q_error_arith;
extern Lisp_Object q_error_file;
extern Lisp_Object q_error_funcargs;
extern Lisp_Object q_error_invalidfunc;
extern Lisp_Object q_error_maxhandlerdepth;
extern Lisp_Object q_error_stackoverflow;
extern Lisp_Object q_error_syntax;
extern Lisp_Object q_error_type;
extern Lisp_Object q_error_unbound;
extern Lisp_Object q_error_unimplemented;
extern Lisp_Object v_obarray;
extern Lisp_Object l_globalenv;

void init_builtins ();

// inlined helpers

static inline int
eq (Lisp_Object x, Lisp_Object y)
{
  return x == y;
}

static inline int
nil (Lisp_Object o)
{
  return eq (o, q_nil);
}

static inline int
unbound (Lisp_Object o)
{
  return eq (o, q_unbound);
}

static inline int
intp (Lisp_Object o)
{
  return tag_of (o) == LISP_TAG_INTG;
}

static inline int
floatp (Lisp_Object o)
{
  // only the lowest subtag bit counts, see Lisp_Immd_Subtag
  return tag_of (o) == LISP_TAG_IMMD
         && ((o >> TAGBITS) & 1) == LISP_TAG_FLOT;
}

static inline int
numberp (Lisp_Object o)
{
  return intp (o) || floatp (o);
}

static inline int
consp (Lisp_Object o)
{
  return tag_of (o) == LISP_TAG_CONS;
}

static inline int
listp (Lisp_Object o)
{
  return nil (o) || consp (o);
}

static inline int
symbolp (Lisp_Object o)
{
  return tag_of (o) == LISP_TAG_SYMB;
}

static inline int
stringp (Lisp_Object o)
{
  return tag_of (o) == LISP_TAG_STRG;
}

static inline int
vectorp (Lisp_Object o)
{
  return tag_of (o) == LISP_TAG_CPLX && complex_type_of (o) == LISP_VECT;
}

static inline int
charp (Lisp_Object o)
{
  return tag_of (o) == LISP_TAG_IMMD
         && ((o >> TAGBITS) & IMMDSUBTAGMASK) == LISP_TAG_CHAR;
}

#endif // LISP_H
