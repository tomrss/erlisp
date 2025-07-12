#include "alloc.h"
#include "env.h"
#include "lisp.h"
#include "obarray.h"

Lisp_Object q_nil;
Lisp_Object q_t;
Lisp_Object q_unbound;
Lisp_Object v_obarray;
Lisp_Object l_globalenv;
/*
  This is a first, simple and naive implementation, but this pointer
  makes multithreading impossible and it's also very ugly.

  TODO: erase this.
 */
Lisp_Object *currentenv;

static void obarray_register_builtins (Lisp_Object obarray);

void
init_builtins ()
{
  q_unbound = make_nstr_symbol ("unbound", 7);
  q_nil = make_nstr_symbol ("nil", 3);
  q_t = make_nstr_symbol ("t", 1);

  v_obarray = obarray_init ();
  obarray_register_builtins (v_obarray);

  l_globalenv = env_init ();
  currentenv = malloc (sizeof (Lisp_Object));
  *currentenv = l_globalenv;
}

static void
obarray_register_builtins (Lisp_Object o)
{
  obarray_put (o, q_nil);
  obarray_put (o, q_t);
  obarray_put (o, q_unbound);

  obarray_put (o, DEFSUBR ("cons", 2, 2, f_cons));
  obarray_put (o, DEFSUBR ("setcar", 2, 2, f_setcar));
  obarray_put (o, DEFSUBR ("setcdr", 2, 2, f_setcdr));
  obarray_put (o, DEFSUBR ("vector", 1, 1, f_vector));
  obarray_put (o, DEFSUBR ("symbol", 1, 1, f_symbol));
  obarray_put (o, DEFSUBR ("symbol_value", 1, 1, f_symbol_value));
  obarray_put (o, DEFSUBR ("car", 1, 1, f_car));
  obarray_put (o, DEFSUBR ("cdr", 1, 1, f_cdr));
  obarray_put (o, DEFSUBR ("eq?", 2, 2, f_eq_p));
  obarray_put (o, DEFSUBR ("equal?", 2, 2, f_equal_p));
  obarray_put (o, DEFSUBR ("eval", 1, 1, f_eval));
  obarray_put (o, DEFSUBR ("assoc", 2, 2, f_assoc));
  obarray_put (o, DEFSUBR ("assq", 2, 2, f_assq));
  obarray_put (o, DEFSUBR ("rassoc", 2, 2, f_rassoc));
  obarray_put (o, DEFSUBR ("rassq", 2, 2, f_rassq));
  obarray_put (o, DEFSUBR ("string=?", 2, 2, f_string_equal_p));
  obarray_put (o, DEFSUBR ("string-length", 1, 1, f_string_length));
  obarray_put (o, DEFSUBR ("length", 1, 1, f_length));
  obarray_put (o, DEFSUBR ("+", 0, MANY, f_sum));
  obarray_put (o, DEFSUBR ("-", 1, MANY, f_subtract));
  obarray_put (o, DEFSUBR ("*", 0, MANY, f_multiply));
  obarray_put (o, DEFSUBR ("/", 1, MANY, f_divide));
  obarray_put (o, DEFSUBR (">", 2, 2, f_ge));
  obarray_put (o, DEFSUBR (">=", 2, 2, f_geq));
  obarray_put (o, DEFSUBR ("<", 2, 2, f_le));
  obarray_put (o, DEFSUBR ("<=", 2, 2, f_leq));
  obarray_put (o, DEFSUBR ("progn", 0, UNEVALLED, f_progn));
  obarray_put (o, DEFSUBR ("quote", 0, UNEVALLED, f_quote));
  obarray_put (o, DEFSUBR ("let*", 1, UNEVALLED, f_let));
  obarray_put (o, DEFSUBR ("let", 1, UNEVALLED, f_let));
  obarray_put (o, DEFSUBR ("lambda", 2, UNEVALLED, f_lambda));
  obarray_put (o, DEFSUBR ("define", 2, UNEVALLED, f_define));
  obarray_put (o, DEFSUBR ("format", 2, MANY, f_format));
  obarray_put (o, DEFSUBR ("gc", 0, 0, f_gc));
  obarray_put (o, DEFSUBR ("memstats", 0, 0, f_memstats));
  obarray_put (o, DEFSUBR ("memdump", 0, 0, f_memdump));
}
