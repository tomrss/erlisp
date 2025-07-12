#include "alloc.h"
#include "blkalloc.h"
#include "debug.h"
#include "env.h"
#include "eval.h"
#include "lisp.h"

static Lisp_Object assoc_w_pred (Lisp_Object key, Lisp_Object alist,
                                 Lisp_Object (*keypred) (Lisp_Object k1,
                                                         Lisp_Object k2));
static Lisp_Object rassoc_w_pred (Lisp_Object key, Lisp_Object alist,
                                  Lisp_Object (*keypred) (Lisp_Object k1,
                                                          Lisp_Object k2));

Lisp_Object
f_symbol (Lisp_Object name)
{
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
  // todo type safety
  unbox_cons (cons)->car = car;
  return cons;
}

Lisp_Object
f_setcdr (Lisp_Object cons, Lisp_Object cdr)
{
  // todo type safety
  unbox_cons (cons)->cdr = cdr;
  return cons;
}

Lisp_Object
f_vector (Lisp_Object size)
{
  return make_vector (unbox_int (size));
}

Lisp_Object
f_symbol_value (Lisp_Object symbol)
{
  if (type_of (symbol) != LISP_SYMB)
    // TODO
    exit (3);
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
  if (type_of (cons) == LISP_CONS)
    return unbox_cons (cons)->car;
  return q_nil;
  // TODO type safety
}

Lisp_Object
f_cdr (Lisp_Object cons)
{
  if (type_of (cons) != LISP_CONS)
    return q_nil;

  return unbox_cons (cons)->cdr;
  // TODO type safety
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
      return f_equal_p (f_car (x), f_car (y));
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
  Lisp_String *ux = unbox_string (x);
  Lisp_String *uy = unbox_string (y);

  if (ux->size == uy->size && strncmp (ux->data, uy->data, ux->size) == 0)
    return q_t;
  return q_nil;
}

Lisp_Object
f_string_length (Lisp_Object string)
{
  return box_int (unbox_string (string)->size);
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

  // TODO err wrong type
  fprintf (stderr, "err wrong type: list, %s\n", type_name (type_of (list)));
  exit (12);
}

Lisp_Object
f_sum (int argc, Lisp_Object *argv)
{
  // TODO type safety
  Lisp_Integer accu = 0;
  for (int i = 0; i < argc; i++)
    accu += unbox_int (argv[i]);
  return box_int (accu);
}

Lisp_Object
f_subtract (int argc, Lisp_Object *argv)
{
  // TODO type safety
  Lisp_Integer accu = argv[0];
  for (int i = 1; i < argc; i++)
    accu -= unbox_int (argv[i]);
  return box_int (accu);
}

Lisp_Object
f_multiply (int argc, Lisp_Object *argv)
{
  // TODO type safety
  Lisp_Integer accu = 1;
  for (int i = 0; i < argc; i++)
    accu *= unbox_int (argv[i]);
  return box_int (accu);
}

Lisp_Object
f_divide (int argc, Lisp_Object *argv)
{
  // TODO type safety
  Lisp_Integer accu = argv[0];
  for (int i = 1; i < argc; i++)
    accu /= unbox_int (argv[i]);
  return box_int (accu);
}

Lisp_Object
f_ge (Lisp_Object x, Lisp_Object y)
{
  return BOOL (unbox_int (x) > unbox_int (y));
}

Lisp_Object
f_geq (Lisp_Object x, Lisp_Object y)
{
  return BOOL (unbox_int (x) >= unbox_int (y));
}

Lisp_Object
f_le (Lisp_Object x, Lisp_Object y)
{
  return BOOL (unbox_int (x) < unbox_int (y));
}

Lisp_Object
f_leq (Lisp_Object x, Lisp_Object y)
{
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
  while (!nil (tail))
    {
      tem = f_car (tail);
      if (!nil (f_eval (f_car (tem))))
        return f_eval (f_car (f_cdr (tem)));
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
      argv[i] = f_car (argtail);
      argtail = f_cdr (argtail);
    }

  // TODO &optional and &re st
  return make_lambda (nargs, nargs, argv, body);
}

Lisp_Object
f_format (int argc, Lisp_Object *argv)
{
  if (argc < 2)
    {
      // TODO err
      printf ("wrong arguments\n");
      return q_nil;
    }

  // TODO support formatting

  Lisp_Object dest = argv[0];
  Lisp_Object fmt = argv[1];
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

  // TODO else print to dest, not supported yet
  fprintf (stderr,
           "print to dest not nil (return str) or t (print stdout) not "
           "supported yet\n");
  return q_nil;
}

Lisp_Object
f_gc ()
{
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

// helpers impl

static Lisp_Object
assoc_w_pred (Lisp_Object key, Lisp_Object alist,
              Lisp_Object (*keypred) (Lisp_Object k1, Lisp_Object k2))
{
  Lisp_Object tail;
  for (tail = alist; !eq (tail, q_nil); tail = f_cdr (tail))
    {
      Lisp_Object elt = f_car (tail);
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
      if (keypred (f_cdr (elt), key) != q_nil)
        return elt;
    }
  return q_nil;
}
