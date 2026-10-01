#include "alloc.h"
#include "env.h"
#include "error.h"
#include "eval.h"
#include "lexer.h"
#include "lisp.h"
#include "obarray.h"
#include "parser.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

Lisp_Object q_nil;
Lisp_Object q_t;
Lisp_Object q_unbound;
Lisp_Object q_error;
Lisp_Object q_error_arith;
Lisp_Object q_error_file;
Lisp_Object q_error_funcargs;
Lisp_Object q_error_invalidfunc;
Lisp_Object q_error_maxhandlerdepth;
Lisp_Object q_error_stackoverflow;
Lisp_Object q_error_type;
Lisp_Object q_error_unbound;
Lisp_Object q_error_unimplemented;
Lisp_Object v_obarray;
Lisp_Object l_globalenv;

Lisp_Object *currentenv;

static void obarray_register_builtins (Lisp_Object obarray);
static Lisp_Object assoc_w_pred (Lisp_Object key, Lisp_Object alist,
                                 Lisp_Object (*keypred) (Lisp_Object k1,
                                                         Lisp_Object k2));
static Lisp_Object rassoc_w_pred (Lisp_Object key, Lisp_Object alist,
                                  Lisp_Object (*keypred) (Lisp_Object k1,
                                                          Lisp_Object k2));

Lisp_Object
f_symbol (Lisp_Object name)
{
  check_type (name, LISP_STRG);

  return make_symbol (name);
}

Lisp_Object
f_cons (Lisp_Object car, Lisp_Object cdr)
{
  return make_cons (car, cdr);
}

Lisp_Object
f_setcar (Lisp_Object cons, Lisp_Object car)
{
  check_type (cons, LISP_CONS);

  unbox_cons (cons)->car = car;
  return cons;
}

Lisp_Object
f_setcdr (Lisp_Object cons, Lisp_Object cdr)
{
  check_type (cons, LISP_CONS);

  unbox_cons (cons)->cdr = cdr;
  return cons;
}

Lisp_Object
f_vector (Lisp_Object size)
{
  check_type (size, LISP_INTG);

  return make_vector (unbox_int (size));
}

Lisp_Object
f_symbol_value (Lisp_Object symbol)
{
  check_type (symbol, LISP_SYMB);

  return eval_symbol (env_current (), symbol);
}

Lisp_Object
f_eval (Lisp_Object form)
{
  return eval (env_current (), form);
}

Lisp_Object
f_car (Lisp_Object cons)
{
  if (nil (cons))
    return q_nil;

  check_type (cons, LISP_CONS);

  return unbox_cons (cons)->car;
}

Lisp_Object
f_cdr (Lisp_Object cons)
{
  if (nil (cons))
    return q_nil;

  check_type (cons, LISP_CONS);

  return unbox_cons (cons)->cdr;
}

Lisp_Object
f_cadr (Lisp_Object cons)
{
  return f_car (f_cdr (cons));
}

Lisp_Object
f_cddr (Lisp_Object cons)
{
  return f_cdr (f_cdr (cons));
}

Lisp_Object
f_assoc (Lisp_Object key, Lisp_Object alist)
{
  return assoc_w_pred (key, alist, &f_equal_p);
}

Lisp_Object
f_assq (Lisp_Object key, Lisp_Object alist)
{
  return assoc_w_pred (key, alist, &f_eq_p);
}

Lisp_Object
f_rassoc (Lisp_Object key, Lisp_Object alist)
{
  return rassoc_w_pred (key, alist, &f_equal_p);
}

Lisp_Object
f_rassq (Lisp_Object key, Lisp_Object alist)
{
  return rassoc_w_pred (key, alist, &f_eq_p);
}

Lisp_Object
f_eq_p (Lisp_Object x, Lisp_Object y)
{
  return eq (x, y) ? q_t : q_nil;
}

Lisp_Object
f_equal_p (Lisp_Object x, Lisp_Object y)
{
  if (x == y)
    // they are the same object, works for every type
    return q_t;

  Lisp_Type type = type_of (x);
  if (type != type_of (y))
    return q_nil;

  switch (type)
    {
    case LISP_INTG:
      // immediates can be directly compared, but should already be
      // handled at beginning of function
      return x == y ? q_t : q_nil;
    case LISP_STRG:
      return f_string_equal_p (x, y);
    case LISP_SYMB:
      return f_string_equal_p (unbox_symbol (x)->name, unbox_symbol (y)->name);
    case LISP_CONS:
      if (nil (f_equal_p (f_car (x), f_car (y))))
        return q_nil;
      return f_equal_p (f_cdr (x), f_cdr (y));
    case LISP_VECT:
      break;
    case LISP_SUBR:
      break;
    case LISP_LMBD:
      break;
    }
  // TODO
  return q_nil;
}

Lisp_Object
f_string_equal_p (Lisp_Object x, Lisp_Object y)
{
  check_type (x, LISP_STRG);
  check_type (y, LISP_STRG);

  Lisp_String *ux = unbox_string (x);
  Lisp_String *uy = unbox_string (y);

  if (ux->size == uy->size && strncmp (ux->data, uy->data, ux->size) == 0)
    return q_t;
  return q_nil;
}

Lisp_Object
f_string_length (Lisp_Object string)
{
  check_type (string, LISP_STRG);

  return box_int (unbox_string (string)->size);
}

Lisp_Object
f_number_to_string (Lisp_Object number)
{
  // TODO other numbers, only int for now
  check_type (number, LISP_INTG);

  Lisp_Integer unumber = unbox_int (number);
  if (unumber == 0)
    return make_string ("0");

  size_t size = floor (log10 (unumber > 0 ? unumber : -unumber)) + 1;
  if (unumber < 0)
    size++; // minus sign

  Lisp_Object string = make_uninit_string (size);

  snprintf (unbox_string (string)->data, size + 1, "%lld", unumber);
  return string;
}

Lisp_Object
f_string_to_number (Lisp_Object string)
{
  check_type (string, LISP_STRG);

  const char *cstr = unbox_string (string)->data;
  char *endptr;
  long num;

  errno = 0;

  num = strtol (cstr, &endptr, 10);

  if (cstr == endptr)
    arith_error_2 ("Unable to parse number", string);
  else if (errno == ERANGE)
    arith_error_2 ("Number of out range", string);
  else if (*endptr != '\0')
    arith_error_2 ("Cannot parse unsafe not null-terminated string", string);
  else
    return box_int (num);
}

Lisp_Object
f_concat (int argc, Lisp_Object *argv)
{
  if (argc == 0)
    return make_uninit_string (0);

  size_t size = 0;
  for (int i = 0; i < argc; i++)
    {
      Lisp_Object string = argv[i];
      check_type (string, LISP_STRG);
      size += unbox_string (string)->size;
    }

  Lisp_Object result = make_uninit_string (size);
  Lisp_String *uresult = unbox_string (result);

  size_t offset = 0;
  for (int i = 0; i < argc; i++)
    {
      Lisp_Object string = argv[i];
      Lisp_String *ustring = unbox_string (string);
      memcpy (uresult->data + offset, ustring->data, ustring->size);
      offset += ustring->size;
    }

  return result;
}

Lisp_Object
f_length (Lisp_Object list)
{
  if (eq (list, q_nil))
    return 0;

  if (type_of (list) == LISP_CONS)
    {
      int64_t length = 0;
      Lisp_Object tail = list;

      while (!eq (tail, q_nil))
        {
          tail = f_cdr (tail);
          length++;
        }

      return box_int (length);
    }

  if (type_of (list) == LISP_STRG)
    return f_string_length (list);

  type_error_2 (list, LISP_CONS, LISP_STRG);
}

Lisp_Object
f_sum (int argc, Lisp_Object *argv)
{
  Lisp_Integer accu = 0;
  for (int i = 0; i < argc; i++)
    {
      // TODO only integers for now
      check_type (argv[i], LISP_INTG);
      accu += unbox_int (argv[i]);
    }
  return box_int (accu);
}

Lisp_Object
f_subtract (int argc, Lisp_Object *argv)
{
  if (argc == 0)
    return box_int (0);

  check_type (argv[0], LISP_INTG);

  if (argc == 1)
    return box_int (-unbox_int (argv[0]));

  Lisp_Integer accu = unbox_int (argv[0]);
  for (int i = 1; i < argc; i++)
    {
      check_type (argv[i], LISP_INTG);
      accu -= unbox_int (argv[i]);
    }
  return box_int (accu);
}

Lisp_Object
f_multiply (int argc, Lisp_Object *argv)
{
  Lisp_Integer accu = 1;
  for (int i = 0; i < argc; i++)
    {
      check_type (argv[i], LISP_INTG);
      accu *= unbox_int (argv[i]);
    }
  return box_int (accu);
}

Lisp_Object
f_divide (int argc, Lisp_Object *argv)
{
  if (argc == 0)
    return box_int (1);

  check_type (argv[0], LISP_INTG);

  if (argc == 1)
    return box_int (1 / unbox_int (argv[0]));

  Lisp_Integer accu = unbox_int (argv[0]);
  for (int i = 1; i < argc; i++)
    {
      check_type (argv[i], LISP_INTG);
      Lisp_Integer divider = unbox_int (argv[i]);
      if (divider == 0)
        arith_error ("division by zero");
      accu /= divider;
    }
  return box_int (accu);
}

Lisp_Object
f_ge (Lisp_Object x, Lisp_Object y)
{
  check_type (x, LISP_INTG);
  check_type (y, LISP_INTG);

  return BOOL (unbox_int (x) > unbox_int (y));
}

Lisp_Object
f_geq (Lisp_Object x, Lisp_Object y)
{
  check_type (x, LISP_INTG);
  check_type (y, LISP_INTG);

  return BOOL (unbox_int (x) >= unbox_int (y));
}

Lisp_Object
f_le (Lisp_Object x, Lisp_Object y)
{
  check_type (x, LISP_INTG);
  check_type (y, LISP_INTG);

  return BOOL (unbox_int (x) < unbox_int (y));
}

Lisp_Object
f_leq (Lisp_Object x, Lisp_Object y)
{
  check_type (x, LISP_INTG);
  check_type (y, LISP_INTG);

  return BOOL (unbox_int (x) <= unbox_int (y));
}

Lisp_Object
f_progn (Lisp_Object form)
{
  return progn (env_current (), form);
}

Lisp_Object
f_quote (Lisp_Object form)
{
  return f_car (form);
}

Lisp_Object
f_let (Lisp_Object form)
{
  return let (env_current (), form);
}

Lisp_Object
f_define (Lisp_Object form)
{
  return define (env_current (), form);
}

Lisp_Object
f_and (Lisp_Object form)
{
  Lisp_Object tail = form;
  Lisp_Object tem;
  while (!nil (tail))
    {
      tem = f_eval (f_car (tail));
      if (nil (tem))
        return q_nil;
      tail = f_cdr (tail);
    }
  return tem;
}

Lisp_Object
f_or (Lisp_Object form)
{
  Lisp_Object tail = form;
  Lisp_Object tem;
  while (!nil (tail))
    {
      tem = f_eval (f_car (tail));
      if (!nil (tem))
        return tem;
      tail = f_cdr (tail);
    }
  return q_nil;
}

Lisp_Object
f_if (Lisp_Object form)
{
  Lisp_Object cond = f_car (form);
  Lisp_Object body = f_cdr (form);

  if (!nil (f_eval (cond)))
    return f_eval (f_car (body));
  else
    return f_progn (f_cdr (body));
}

Lisp_Object
f_when (Lisp_Object form)
{
  Lisp_Object cond = f_car (form);
  Lisp_Object body = f_cdr (form);

  if (!nil (f_eval (cond)))
    return f_progn (body);

  return q_nil;
}

Lisp_Object
f_unless (Lisp_Object form)
{
  Lisp_Object cond = f_car (form);
  Lisp_Object body = f_cdr (form);

  if (nil (f_eval (cond)))
    return f_progn (body);

  return q_nil;
}

Lisp_Object
f_cond (Lisp_Object form)
{
  Lisp_Object tail = form;
  Lisp_Object tem;
  Lisp_Object result = q_nil;
  while (!nil (tail))
    {
      tem = f_car (tail);
      if (!nil (f_eval (f_car (tem))))
        {
          Lisp_Object bodyform = f_cdr (tem);
          Lisp_Object bodyformtail = bodyform;
          while (!nil (bodyformtail))
            {
              Lisp_Object evalled = f_eval (f_car (bodyformtail));
              result = evalled;
              bodyformtail = f_cdr (bodyformtail);
            }

          return result;
        }
      tail = f_cdr (tail);
    }
  return q_nil;
}

Lisp_Object
f_lambda (Lisp_Object form)
{
  Lisp_Object args = f_car (form);
  Lisp_Object body = f_cdr (form);

  size_t nargs = unbox_int (f_length (args));
  Lisp_Object *argv = malloc (nargs * sizeof (Lisp_Object));

  Lisp_Object argtail = args;
  for (size_t i = 0; i < nargs; i++)
    {
      Lisp_Object argsym = f_car (argtail);
      check_type (argsym, LISP_SYMB);

      // dirty trick to optimize lookups of purely global symbols
      unbox_symbol (argsym)->localbound = 1;

      argv[i] = argsym;
      argtail = f_cdr (argtail);
    }

  // TODO &optional and &re st
  return make_lambda (nargs, nargs, env_current (), argv, body);
}

Lisp_Object
f_format (int argc, Lisp_Object *argv)
{
  // TODO support formatting
  (void)argc; // unused for now

  Lisp_Object dest = argv[0];
  Lisp_Object fmt = argv[1];
  check_type (fmt, LISP_STRG);
  Lisp_String *ufmt = unbox_string (fmt);

  if (eq (dest, q_t))
    {
      // print to stdout
      fwrite (ufmt->data, sizeof (char), ufmt->size, stdout);
      printf ("\n");
      return q_nil;
    }
  if (eq (dest, q_nil))
    {
      // return fmt string
      return fmt;
    }

  unimplemented_error (
      "print to dest not nil (return str) or t (print stdout)");
}

Lisp_Object
f_load (Lisp_Object path)
{
  check_type (path, LISP_STRG);

  const char *upath = unbox_string (path)->data;

  FILE *f = fopen (upath, "r");
  if (!f)
    {
      file_error (path);
    }

  Lexer *l = lex_init (stream_file (f));

  Lisp_Object form;
  while (parse_next_sexp (l, &form))
    {
      // load evaluates always in global scope:
      // that's how it's done in emacs and scheme
      eval (l_globalenv, form);
    }

  lex_close (l);
  fclose (f);

  return q_t;
}

NORETURN Lisp_Object
f_signal (Lisp_Object symbol, Lisp_Object data)
{
  xsignal (symbol, data);
}

Lisp_Object
f_error_symbol (Lisp_Object err)
{
  return f_car (err);
}

Lisp_Object
f_error_backtrace (Lisp_Object err)
{
  return f_cadr (err);
}

Lisp_Object
f_error_data (Lisp_Object err)
{
  return f_cddr (err);
}

Lisp_Object
f_intern (Lisp_Object name)
{
  check_type (name, LISP_STRG);

  Lisp_Object symbol;
  symbol = obarray_lookup_name (v_obarray, name);
  if (type_of (symbol) == LISP_SYMB)
    return symbol;

  symbol = make_symbol (name);
  obarray_put (v_obarray, symbol);
  return symbol;
}

Lisp_Object
f_gc (Lisp_Object printmemstats)
{
  if (nil (printmemstats))
    {
      gc ();
      return q_nil;
    }

  printf ("before GC:\n");
  print_memstats (memstats ());
  struct memstats stats = gc ();
  printf ("after GC:\n");
  print_memstats (stats);
  return q_nil;
}

Lisp_Object
f_memdump ()
{
  memdump ();
  return q_nil;
}

Lisp_Object
f_memstats ()
{
  // TODO return lisp object containing info, not print stdout
  print_memstats (memstats ());
  return q_nil;
}

void
init_builtins ()
{
  q_unbound = make_nstr_symbol ("unbound", 7);
  q_nil = make_nstr_symbol ("nil", 3);
  q_t = make_nstr_symbol ("t", 1);
  q_error = make_nstr_symbol ("error", 5);
  q_error_arith = make_str_symbol ("arith-error");
  q_error_file = make_str_symbol ("file-error");
  q_error_funcargs = make_str_symbol ("func-args-error");
  q_error_invalidfunc = make_str_symbol ("invalid-func-error");
  q_error_maxhandlerdepth = make_str_symbol ("max-handler-depth-error");
  q_error_stackoverflow = make_str_symbol ("stack-overflow-error");
  q_error_type = make_str_symbol ("type-error");
  q_error_unbound = make_str_symbol ("unbound-error");
  q_error_unimplemented = make_str_symbol ("unimplemented-error");

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
  obarray_put (o, q_error_arith);
  obarray_put (o, q_error_file);
  obarray_put (o, q_error_funcargs);
  obarray_put (o, q_error_invalidfunc);
  obarray_put (o, q_error_maxhandlerdepth);
  obarray_put (o, q_error_stackoverflow);
  obarray_put (o, q_error_type);
  obarray_put (o, q_error_unbound);
  obarray_put (o, q_error_unimplemented);

  obarray_put (o, DEFSUBR ("cons", 2, 2, f_cons));
  obarray_put (o, DEFSUBR ("setcar", 2, 2, f_setcar));
  obarray_put (o, DEFSUBR ("setcdr", 2, 2, f_setcdr));
  obarray_put (o, DEFSUBR ("vector", 1, 1, f_vector));
  obarray_put (o, DEFSUBR ("symbol", 1, 1, f_symbol));
  obarray_put (o, DEFSUBR ("symbol_value", 1, 1, f_symbol_value));
  obarray_put (o, DEFSUBR ("car", 1, 1, f_car));
  obarray_put (o, DEFSUBR ("cdr", 1, 1, f_cdr));
  obarray_put (o, DEFSUBR ("cadr", 1, 1, f_cadr));
  obarray_put (o, DEFSUBR ("cddr", 1, 1, f_cddr));
  obarray_put (o, DEFSUBR ("eq?", 2, 2, f_eq_p));
  obarray_put (o, DEFSUBR ("equal?", 2, 2, f_equal_p));
  obarray_put (o, DEFSUBR ("eval", 1, 1, f_eval));
  obarray_put (o, DEFSUBR ("assoc", 2, 2, f_assoc));
  obarray_put (o, DEFSUBR ("assq", 2, 2, f_assq));
  obarray_put (o, DEFSUBR ("rassoc", 2, 2, f_rassoc));
  obarray_put (o, DEFSUBR ("rassq", 2, 2, f_rassq));
  obarray_put (o, DEFSUBR ("string=?", 2, 2, f_string_equal_p));
  obarray_put (o, DEFSUBR ("string-length", 1, 1, f_string_length));
  obarray_put (o, DEFSUBR ("number->string", 1, 1, f_number_to_string));
  obarray_put (o, DEFSUBR ("string->number", 1, 1, f_string_to_number));
  obarray_put (o, DEFSUBR ("concat", 0, MANY, f_concat));
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
  obarray_put (o, DEFSUBR ("and", 1, UNEVALLED, f_and));
  obarray_put (o, DEFSUBR ("or", 1, UNEVALLED, f_or));
  obarray_put (o, DEFSUBR ("if", 2, UNEVALLED, f_if));
  obarray_put (o, DEFSUBR ("when", 2, UNEVALLED, f_when));
  obarray_put (o, DEFSUBR ("unless", 2, UNEVALLED, f_unless));
  obarray_put (o, DEFSUBR ("cond", 1, UNEVALLED, f_cond));
  obarray_put (o, DEFSUBR ("lambda", 2, UNEVALLED, f_lambda));
  obarray_put (o, DEFSUBR ("define", 2, UNEVALLED, f_define));
  obarray_put (o, DEFSUBR ("format", 2, MANY, f_format));
  obarray_put (o, DEFSUBR ("load", 1, 1, f_load));
  obarray_put (o, DEFSUBR ("signal", 1, 2, f_signal));
  obarray_put (o, DEFSUBR ("error-symbol", 1, 1, f_error_symbol));
  obarray_put (o, DEFSUBR ("error-backtrace", 1, 1, f_error_backtrace));
  obarray_put (o, DEFSUBR ("error-data", 1, 1, f_error_data));
  obarray_put (o, DEFSUBR ("gc", 0, 1, f_gc));
  obarray_put (o, DEFSUBR ("memstats", 0, 0, f_memstats));
  obarray_put (o, DEFSUBR ("memdump", 0, 0, f_memdump));
}

// helpers impl

static Lisp_Object
assoc_w_pred (Lisp_Object key, Lisp_Object alist,
              Lisp_Object (*keypred) (Lisp_Object k1, Lisp_Object k2))
{
  Lisp_Object tail;
  for (tail = alist; !nil (tail); tail = f_cdr (tail))
    {
      Lisp_Object elt = f_car (tail);
      if (type_of (elt) != LISP_CONS)
        // ignore it: emacs does this, other lisps error. i like this
        // because its flexible and easy to implement
        continue;
      if (keypred (f_car (elt), key) != q_nil)
        return elt;
    }
  return q_nil;
}

static Lisp_Object
rassoc_w_pred (Lisp_Object key, Lisp_Object alist,
               Lisp_Object (*keypred) (Lisp_Object k1, Lisp_Object k2))
{
  Lisp_Object tail;
  for (tail = alist; tail != q_nil; tail = f_cdr (tail))
    {
      Lisp_Object elt = f_car (tail);
      if (type_of (elt) != LISP_CONS)
        // ignore it: emacs does this, other lisps error. i like this
        // because its flexible and easy to implement
        continue;
      if (keypred (f_cdr (elt), key) != q_nil)
        return elt;
    }
  return q_nil;
}
