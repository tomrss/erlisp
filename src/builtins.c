#include "alloc.h"
#include "env.h"
#include "error.h"
#include "eval.h"
#include "lexer.h"
#include "lisp.h"
#include "obarray.h"
#include "parser.h"
#include "prelude.h"
#include "print.h"
#include "stack.h"
#include <alloca.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

Lisp_Object q_nil;
Lisp_Object q_t;
Lisp_Object q_unbound;
Lisp_Object q_eof;
Lisp_Object q_quote;
Lisp_Object q_quasiquote;
Lisp_Object q_unquote;
Lisp_Object q_unquote_splicing;
Lisp_Object q_lambda;
Lisp_Object q_define;
Lisp_Object q_eval;
Lisp_Object q_cons;
Lisp_Object q_error;
Lisp_Object q_error_arith;
Lisp_Object q_error_file;
Lisp_Object q_error_funcargs;
Lisp_Object q_error_funcinvalid;
Lisp_Object q_error_maxhandlerdepth;
Lisp_Object q_error_portclosed;
Lisp_Object q_error_portinvalid;
Lisp_Object q_error_portio;
Lisp_Object q_error_stackoverflow;
Lisp_Object q_error_syntax;
Lisp_Object q_error_type;
Lisp_Object q_error_unbound;
Lisp_Object q_error_unimplemented;
Lisp_Object v_obarray;
Lisp_Object p_current_input_port;
Lisp_Object p_current_output_port;
Lisp_Object l_globalenv;

typedef Lisp_Object (*lisp_2_pred) (Lisp_Object x, Lisp_Object y);

static void obarray_register_builtins (Lisp_Object obarray);
static Lisp_Object assoc_w_pred (Lisp_Object key, Lisp_Object alist,
                                 lisp_2_pred keypred);
static Lisp_Object rassoc_w_pred (Lisp_Object key, Lisp_Object alist,
                                  lisp_2_pred keypred);
static Lisp_Object member_w_pred (Lisp_Object elt, Lisp_Object lst,
                                  lisp_2_pred eltpred);

static inline Lisp_Port *safe_unbox_optionl_in_port (Lisp_Object port);
static inline Lisp_Port *safe_unbox_optionl_out_port (Lisp_Object port);

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
f_eval_noexpand (Lisp_Object form)
{
  return eval (l_globalenv, form);
}

Lisp_Object
f_apply (Lisp_Object fun, Lisp_Object args)
{
  return apply (env_current (), fun, args);
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
f_member (Lisp_Object elt, Lisp_Object list)
{
  return member_w_pred (elt, list, &f_equal_p);
}

Lisp_Object
f_memq (Lisp_Object elt, Lisp_Object list)
{
  return member_w_pred (elt, list, &f_eq_p);
}

Lisp_Object
f_append (Lisp_Object list, Lisp_Object o)
{
  if (nil (list))
    return o;

  Lisp_Object result = q_nil;
  Lisp_Object resulttail = q_nil;
  Lisp_Object tail = list;
  while (!nil (tail))
    {
      Lisp_Object cell = f_cons (f_car (tail), q_nil);
      if (nil (result))
        result = cell;
      else
        f_setcdr (resulttail, cell);
      resulttail = cell;
      tail = f_cdr (tail);
    }

  f_setcdr (resulttail, o);
  return result;
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
    case LISP_CHAR:
    case LISP_FLOT:
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
    case LISP_PORT:
      break;
    }
  // TODO
  return q_nil;
}

Lisp_Object
f_eqv_p (Lisp_Object x, Lisp_Object y)
{
  // at the moment with no big num and no floats in heap, eq is sufficient
  return f_eq_p (x, y);
}

Lisp_Object
f_number_p (Lisp_Object o)
{
  return BOOL (numberp (o));
}

Lisp_Object
f_list_p (Lisp_Object o)
{
  return BOOL (listp (o));
}

Lisp_Object
f_cons_p (Lisp_Object o)
{
  return BOOL (consp (o));
}

Lisp_Object
f_symbol_p (Lisp_Object o)
{
  return BOOL (symbolp (o));
}

Lisp_Object
f_string_p (Lisp_Object o)
{
  return BOOL (stringp (o));
}

Lisp_Object
f_vector_p (Lisp_Object o)
{
  return BOOL (vectorp (o));
}

Lisp_Object
f_char_p (Lisp_Object o)
{
  return BOOL (charp (o));
}

Lisp_Object
f_nil_p (Lisp_Object o)
{
  return BOOL (nil (o));
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
  size_t size = snprintf (NULL, 0, "%lld", unumber);

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
f_string_ref (Lisp_Object string, Lisp_Object i)
{
  check_type (string, LISP_STRG);
  check_type (i, LISP_INTG);

  Lisp_Integer ui = unbox_int (i);
  Lisp_String *ustring = unbox_string (string);

  if (ui < 0 || ui >= (Lisp_Integer)ustring->size)
    // TODO out of bound error
    xsignal (q_error, f_cons (make_string ("index out of bound"), q_nil));

  return box_char ((unsigned char)ustring->data[ui]);
}

Lisp_Object
f_symbol_to_string (Lisp_Object symbol)
{
  check_type (symbol, LISP_SYMB);
  // this is correct because strings are immutable, otherwise we should copy
  return unbox_symbol (symbol)->name;
}

Lisp_Object
f_string_to_symbol (Lisp_Object string)
{
  return f_intern (string);
}

Lisp_Object
f_length (Lisp_Object list)
{
  if (nil (list))
    return box_int (0);

  if (consp (list))
    {
      int64_t length = 0;
      Lisp_Object tail = list;

      while (!nil (tail))
        {
          tail = f_cdr (tail);
          length++;
        }

      return box_int (length);
    }

  if (stringp (list))
    return f_string_length (list);

  type_error_2 (list, LISP_CONS, LISP_STRG);
}

static inline Lisp_Float
to_float (Lisp_Object num)
{
  if (intp (num))
    return (Lisp_Float)unbox_int (num);
  if (floatp (num))
    return unbox_float (num);
  type_error_2 (num, LISP_INTG, LISP_FLOT);
}

enum arith_op
{
  OP_SUM,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_MIN,
  OP_MAX,
};

static inline Lisp_Integer
int_op (enum arith_op op, Lisp_Integer a, Lisp_Integer b)
{
  switch (op)
    {
    case OP_SUM:
      return a + b;
    case OP_SUB:
      return a - b;
    case OP_MUL:
      return a * b;
    case OP_DIV:
      if (b == 0)
        arith_error ("division by zero");
      return a / b;
    case OP_MIN:
      return a <= b ? a : b;
    case OP_MAX:
      return a >= b ? a : b;
    }
  internal_error ("arith op %d not implemented", op);
}

static inline Lisp_Float
float_op (enum arith_op op, Lisp_Float a, Lisp_Float b)
{
  switch (op)
    {
    case OP_SUM:
      return a + b;
    case OP_SUB:
      return a - b;
    case OP_MUL:
      return a * b;
    case OP_DIV:
      if (b == 0.f)
        arith_error ("division by zero");
      return a / b;
    case OP_MIN:
      return a <= b ? a : b;
    case OP_MAX:
      return a >= b ? a : b;
    }
  internal_error ("arith op %d not implemented", op);
}

static Lisp_Object
arith_fold (enum arith_op op, int argc, Lisp_Object *argv)
{
  int i = 1;
  Lisp_Integer iaccu;
  Lisp_Float faccu;

  if (intp (argv[0]))
    {
      // accumulate to integer until the first float
      iaccu = unbox_int (argv[0]);
      while (i < argc && intp (argv[i]))
        {
          iaccu = int_op (op, iaccu, unbox_int (argv[i]));
          i++;
        }
      if (i == argc)
        // there where no floats
        return box_int (iaccu);
      // from now on, we will be reasoning only on floats
      faccu = (Lisp_Float)iaccu;
    }
  else
    faccu = to_float (argv[0]);

  while (i < argc)
    {
      faccu = float_op (op, faccu, to_float (argv[i]));
      i++;
    }

  return box_float (faccu);
}

Lisp_Object
f_sum (int argc, Lisp_Object *argv)
{
  if (argc == 0)
    return box_int (0);
  return arith_fold (OP_SUM, argc, argv);
}

Lisp_Object
f_subtract (int argc, Lisp_Object *argv)
{
  // argc > 0 because of minargs is 1
  if (argc == 1)
    {
      if (intp (argv[0]))
        return box_int (-unbox_int (argv[0]));
      if (floatp (argv[0]))
        return box_float (-unbox_float (argv[0]));
      type_error_2 (argv[0], LISP_INTG, LISP_FLOT);
    }

  return arith_fold (OP_SUB, argc, argv);
}

Lisp_Object
f_multiply (int argc, Lisp_Object *argv)
{
  if (argc == 0)
    return box_int (1);

  return arith_fold (OP_MUL, argc, argv);
}

Lisp_Object
f_divide (int argc, Lisp_Object *argv)
{
  // argc > 0 because of minargs is 1
  if (argc == 1)
    {
      if (intp (argv[0]))
        {
          if (unbox_int (argv[0]) == 0)
            arith_error ("division by zero");
          return box_int (1 / unbox_int (argv[0]));
        }
      if (floatp (argv[0]))
        {
          if (unbox_float (argv[0]) == 0.f)
            arith_error ("division by zero");
          return box_float (1.f / unbox_float (argv[0]));
        }
      type_error_2 (argv[0], LISP_INTG, LISP_FLOT);
    }

  return arith_fold (OP_DIV, argc, argv);
}

Lisp_Object
f_min (int argc, Lisp_Object *argv)
{
  return arith_fold (OP_MIN, argc, argv);
}

Lisp_Object
f_max (int argc, Lisp_Object *argv)
{
  return arith_fold (OP_MAX, argc, argv);
}

Lisp_Object
f_abs (Lisp_Object num)
{
  if (intp (num))
    return unbox_int (num) >= 0 ? num : box_int (-unbox_int (num));
  if (floatp (num))
    // sign bit is top bit of the word, clear it
    return num & ~((Lisp_Object)1 << (FLOTSHIFT + 31));
  type_error_2 (num, LISP_INTG, LISP_FLOT);
}

Lisp_Object
f_num_eq (Lisp_Object x, Lisp_Object y)
{
  if (intp (x) && intp (y))
    return BOOL (x == y);

  return BOOL (to_float (x) == to_float (y));
}

Lisp_Object
f_ge (Lisp_Object x, Lisp_Object y)
{
  if (intp (x) && intp (y))
    return BOOL (unbox_int (x) > unbox_int (y));

  return BOOL (to_float (x) > to_float (y));
}

Lisp_Object
f_geq (Lisp_Object x, Lisp_Object y)
{
  if (intp (x) && intp (y))
    return BOOL (unbox_int (x) >= unbox_int (y));

  return BOOL (to_float (x) >= to_float (y));
}

Lisp_Object
f_le (Lisp_Object x, Lisp_Object y)
{
  if (intp (x) && intp (y))
    return BOOL (unbox_int (x) < unbox_int (y));

  return BOOL (to_float (x) < to_float (y));
}

Lisp_Object
f_leq (Lisp_Object x, Lisp_Object y)
{
  if (intp (x) && intp (y))
    return BOOL (unbox_int (x) <= unbox_int (y));

  return BOOL (to_float (x) <= to_float (y));
}

Lisp_Object
f_defined_p (Lisp_Object symbol)
{
  check_type (symbol, LISP_SYMB);
  if (!unbound (env_lookup (env_current (), symbol)))
    return q_t;
  return BOOL (!unbound (unbox_symbol (symbol)->value));
}

Lisp_Object
f_format (int argc, Lisp_Object *argv)
{

  Lisp_Object dest = argv[0];
  Lisp_Port *p;
  if (eq (q_t, dest))
    p = unbox_port (p_current_output_port);
  else if (eq (q_nil, dest))
    p = unbox_port (f_open_output_string ());
  else
    p = safe_unbox_optionl_out_port (dest);

  Lisp_Object fmt = argv[1];
  check_type (fmt, LISP_STRG);
  Lisp_String *ufmt = unbox_string (fmt);

  char *cur = ufmt->data;
  int argind = 2; // 0->dest, 1->fmt
  while (cur[0] != '\0')
    {
      size_t len = strcspn (cur, "~");
      port_write_nstring (cur, len, p);
      cur += len;
      if (cur[0] == '~')
        {
          // simple SRFI-28 impl
          // https://srfi.schemers.org/srfi-28/srfi-28.html
          cur += 1;
          switch (cur[0])
            {
            case 'a':
              if (argind >= argc)
                f_signal (q_error,
                          f_cons (make_string ("invelid fmt"),
                                  f_cons (box_int (argind),
                                          f_cons (box_int (argc), q_nil))));
              fprint_form (p, argv[argind++], 1);
              break;
            case 's':
              if (argind >= argc)
                // todo
                f_signal (q_error,
                          f_cons (make_string ("invelid fmt"), q_nil));
              fprint_form (p, argv[argind++], 0);
              break;
            case '%':
              port_write_char ('\n', p);
              break;
            case '~':
              port_write_char ('~', p);
              break;
            default:
              // todo
              f_signal (q_error,
                        f_cons (make_string ("invelid fmt str"), q_nil));
            }
          cur += 1;
        }
    }
  if (argind != argc)
    // todo this check could be useless/harmful? runs in case of more fmt args
    // than fmt ~ directives todo err
    f_signal (q_error, f_cons (make_string ("invelid fmt END"), q_nil));

  if (eq (q_nil, dest)) // local port
    return f_get_output_string (box_port (p));

  return q_nil;
}

Lisp_Object
f_read (Lisp_Object port)
{
  Lisp_Port *p = safe_unbox_optionl_in_port (port);
  Lexer l = lex_init (p);
  return parse_sexp (&l);
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
  if (symbolp (symbol))
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

static inline Lisp_Port *
safe_unbox_optionl_in_port (Lisp_Object port)
{
  if (nil (port))
    port = p_current_input_port;
  else
    check_type (port, LISP_PORT);
  Lisp_Port *p = unbox_port (port);
  if (p->direction != PORT_IN)
    invalidportdir_error (port, PORT_IN);
  return p;
}

static inline Lisp_Port *
safe_unbox_optionl_out_port (Lisp_Object port)
{
  if (nil (port))
    port = p_current_output_port;
  else
    check_type (port, LISP_PORT);
  Lisp_Port *p = unbox_port (port);
  if (p->direction != PORT_OUT)
    invalidportdir_error (port, PORT_OUT);
  return p;
}

Lisp_Object
f_input_port_p (Lisp_Object o)
{
  if (!portp (o))
    return q_nil;

  return BOOL (unbox_port (o)->direction == PORT_IN);
}

Lisp_Object
f_output_port_p (Lisp_Object o)
{
  if (!portp (o))
    return q_nil;

  return BOOL (unbox_port (o)->direction == PORT_OUT);
}

Lisp_Object
f_eof_object_p (Lisp_Object o)
{
  return f_eq_p (o, q_eof);
}

Lisp_Object
f_current_input_port ()
{
  return p_current_input_port;
}

Lisp_Object
f_current_output_port ()
{
  return p_current_output_port;
}

Lisp_Object
f_set_current_input_port (Lisp_Object port)
{
  check_type (port, LISP_PORT);
  if (unbox_port (port)->direction != PORT_IN)
    invalidportdir_error (port, PORT_IN);
  p_current_input_port = port;
  return port;
}

Lisp_Object
f_set_current_output_port (Lisp_Object port)
{
  check_type (port, LISP_PORT);
  if (unbox_port (port)->direction != PORT_OUT)
    invalidportdir_error (port, PORT_OUT);
  p_current_output_port = port;
  return port;
}

Lisp_Object
f_open_input_file (Lisp_Object path)
{
  check_type (path, LISP_STRG);
  FILE *f = fopen (unbox_string (path)->data, "r");
  if (!f)
    file_error (path);
  return make_file_port (f, PORT_IN);
}

Lisp_Object
f_open_output_file (Lisp_Object path)
{
  check_type (path, LISP_STRG);
  FILE *f = fopen (unbox_string (path)->data, "w");
  if (!f)
    file_error (path);
  return make_file_port (f, PORT_OUT);
}

Lisp_Object
f_open_input_string (Lisp_Object string)
{
  check_type (string, LISP_STRG);
  return make_input_string_port (string);
}

Lisp_Object
f_open_output_string ()
{
  return make_output_string_port ();
}

Lisp_Object
f_get_output_string (Lisp_Object port)
{
  check_type (port, LISP_PORT);
  Lisp_Port *p = unbox_port (port);
  if (p->type != PORT_STRING)
    invalidporttype_error (port, PORT_STRING);
  if (p->direction != PORT_OUT)
    invalidportdir_error (port, PORT_OUT);
  if (p->closed)
    closedport_error (port);

  // this will copy the buf
  return make_nstring (p->backend.string.data, p->backend.string.pos);
}

static inline void
snprintf_addr (struct sockaddr *addr, char *buf, size_t size)
{
  char ip6[INET6_ADDRSTRLEN]; // ip4 will fit here
  struct sockaddr_in *addr_in;
  struct sockaddr_in6 *addr_in6;
  struct sockaddr_un *addr_un;

  switch (addr->sa_family)
    {
    case AF_INET:
      addr_in = (struct sockaddr_in *)addr;
      inet_ntop (AF_INET, &addr_in->sin_addr, ip6, sizeof (ip6));
      snprintf (buf, size, "%s:%d", ip6, ntohs (addr_in->sin_port));
      break;
    case AF_INET6:
      addr_in6 = (struct sockaddr_in6 *)addr;
      inet_ntop (AF_INET6, &addr_in6->sin6_addr, ip6, sizeof (ip6));
      snprintf (buf, size, "[%s]:%d", ip6, ntohs (addr_in6->sin6_port));
      break;
    case AF_UNIX:
      addr_un = (struct sockaddr_un *)addr;
      snprintf (buf, size, "%s", addr_un->sun_path);
      break;
    default:
      snprintf (buf, size, "[unknown socket type");
    }
}

static inline Lisp_Object
connect_socket (int fd, struct sockaddr *addr, socklen_t len)
{
  if (connect (fd, addr, len) < 0)
    {
      int err = errno;
      close (fd);
      char buf[256] = { 0 };
      snprintf_addr (addr, buf, sizeof (buf));
      portio_error (buf, err);
    }

  // we open a couple of FILEs that target the same socket underneath.
  // this is because r+ file on socket is not doable. this will be ditched once
  // we have non-blocking sockets
  FILE *in = fdopen (fd, "r");
  FILE *out = fdopen (dup (fd), "w");

  return f_cons (make_file_port (in, PORT_IN), make_file_port (out, PORT_OUT));
}

Lisp_Object
f_tcp_connect (Lisp_Object host, Lisp_Object port)
{
  check_type (host, LISP_STRG);
  check_type (port, LISP_INTG);

  if (unbox_int (port) < 0 || unbox_int (port) > 65535)
    f_signal (q_error, f_cons (make_string ("port out of range"),
                               f_cons (port, q_nil)));

  int fd = socket (AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    portio_error ("tcp-socket", errno);

  struct sockaddr_in addr;
  memset (&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons (unbox_int (port));
  if (inet_pton (AF_INET, unbox_string (host)->data, &addr.sin_addr) < 1)
    {
      close (fd);
      f_signal (q_error_portio, f_cons (host, f_cons (port, q_nil)));
    }

  return connect_socket (fd, (struct sockaddr *)&addr,
                         sizeof (struct sockaddr_in));
}

Lisp_Object
f_unix_connect (Lisp_Object path)
{
  check_type (path, LISP_STRG);

  int fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    portio_error ("unix-socket", errno);

  struct sockaddr_un addr;
  memset (&addr, 0, sizeof addr);
  addr.sun_family = AF_UNIX;
  strncpy (addr.sun_path, unbox_string (path)->data, sizeof addr.sun_path - 1);

  return connect_socket (fd, (struct sockaddr *)&addr,
                         sizeof (struct sockaddr_un));
}

int
port_read_char (Lisp_Port *p)
{
  if (p->closed)
    closedport_error (box_port (p));

  if (p->current != PORT_NOPEEK)
    {
      int res = p->current;
      p->current = PORT_NOPEEK;
      return res;
    }

  return p->get_char (p);
}

int
port_peek_char (Lisp_Port *p)
{
  if (p->closed)
    closedport_error (box_port (p));

  if (p->current != PORT_NOPEEK)
    return p->current;

  int c = p->get_char (p);
  p->current = c;
  return c;
}

static inline int
port_unsafe_write_char (int c, Lisp_Port *p)
{
  int w = p->put_char (c, p);
  if (w == EOF)
    // TODO this is not a file error
    file_error (make_string ("error writing to output port"));
  return w;
}

int
port_write_char (int c, Lisp_Port *p)
{
  if (p->closed)
    closedport_error (box_port (p));

  return port_unsafe_write_char (c, p);
}

void
port_write_nstring (const char *string, size_t size, Lisp_Port *p)
{
  if (p->closed)
    closedport_error (box_port (p));

  for (size_t i = 0; i < size; i++)
    port_unsafe_write_char (string[i], p);
}

void
port_write_string (const char *string, Lisp_Port *p)
{
  if (p->closed)
    closedport_error (box_port (p));

  int i = 0;
  char c;
  while ((c = string[i++]) != '\0')
    port_unsafe_write_char (c, p);
}

Lisp_Object
f_close_port (Lisp_Object port)
{
  check_type (port, LISP_PORT);
  Lisp_Port *p = unbox_port (port);
  return BOOL (p->close (p));
}

Lisp_Object
f_read_char (Lisp_Object port)
{
  Lisp_Port *p = safe_unbox_optionl_in_port (port);
  int c = port_read_char (p);
  if (c == EOF)
    return q_eof;
  return box_char ((unsigned char)c);
}

Lisp_Object
f_peek_char (Lisp_Object port)
{
  Lisp_Port *p = safe_unbox_optionl_in_port (port);
  int c = port_peek_char (p);
  if (c == EOF)
    return q_eof;
  return box_char ((unsigned char)c);
}

Lisp_Object
f_char_ready_p (Lisp_Object port)
{
  Lisp_Port *p = safe_unbox_optionl_in_port (port);
  if (p->closed)
    closedport_error (port);

  // TODO this will not be the case when we add sockets
  return q_t;
}

Lisp_Object
f_write_char (Lisp_Object c, Lisp_Object port)
{
  check_type (c, LISP_CHAR);
  Lisp_Port *p = safe_unbox_optionl_out_port (port);
  int w = port_write_char (unbox_char (c), p);
  return box_char ((unsigned char)w);
}

Lisp_Object
f_write (Lisp_Object obj, Lisp_Object port)
{
  Lisp_Port *p = safe_unbox_optionl_out_port (port);
  fprint_form (p, obj, 0);
  return q_nil;
}

Lisp_Object
f_display (Lisp_Object obj, Lisp_Object port)
{
  Lisp_Port *p = safe_unbox_optionl_out_port (port);
  fprint_form (p, obj, 1);
  return q_nil;
}

Lisp_Object
f_flush_output_port (Lisp_Object port)
{
  Lisp_Port *p = safe_unbox_optionl_out_port (port);
  if (p->closed)
    closedport_error (port);

  if (p->type == PORT_FILE)
    fflush (p->backend.file);
  return q_nil;
}

static Lisp_Object
special_form (const char *name, enum special_form sf)
{
  Lisp_Object symbol = make_str_symbol (name);
  unbox_symbol (symbol)->special = sf;
  return symbol;
}

void
init_builtins ()
{
  q_nil = make_nstr_symbol ("nil", 3);
  q_t = make_nstr_symbol ("t", 1);
  q_unbound = make_nstr_symbol ("unbound", 7);
  q_eof = make_nstr_symbol ("eof", 3);
  q_quote = special_form ("quote", SF_QUOTE);
  q_lambda = special_form ("lambda", SF_LAMBDA);
  q_define = special_form ("define", SF_DEFINE);
  // bound by the prelude to the eval that expands macros
  q_eval = make_str_symbol ("eval");
  q_quasiquote = make_str_symbol ("quasiquote");
  q_unquote = make_str_symbol ("unquote");
  q_unquote_splicing = make_str_symbol ("unquote-splicing");
  q_error = make_nstr_symbol ("error", 5);
  q_error_arith = make_str_symbol ("arith-error");
  q_error_file = make_str_symbol ("file-error");
  q_error_funcargs = make_str_symbol ("func-args-error");
  q_error_funcinvalid = make_str_symbol ("invalid-func-error");
  q_error_maxhandlerdepth = make_str_symbol ("max-handler-depth-error");
  q_error_portclosed = make_str_symbol ("port-closed-error");
  q_error_portinvalid = make_str_symbol ("port-invalid-error");
  q_error_portio = make_str_symbol ("port-io-error");
  q_error_stackoverflow = make_str_symbol ("stack-overflow-error");
  q_error_syntax = make_str_symbol ("syntax-error");
  q_error_type = make_str_symbol ("type-error");
  q_error_unbound = make_str_symbol ("unbound-error");
  q_error_unimplemented = make_str_symbol ("unimplemented-error");

  v_obarray = obarray_init ();
  obarray_register_builtins (v_obarray);

  p_current_input_port = make_file_port (stdin, PORT_IN);
  p_current_output_port = make_file_port (stdout, PORT_OUT);

  l_globalenv = env_new (q_nil);
  stack_init (l_globalenv);

  init_prelude ();
}

static void
obarray_register_builtins (Lisp_Object o)
{
  obarray_put (o, q_nil);
  obarray_put (o, q_t);
  obarray_put (o, q_quote);
  obarray_put (o, q_lambda);
  obarray_put (o, q_define);
  obarray_put (o, q_eval);
  obarray_put (o, special_form ("if", SF_IF));
  obarray_put (o, special_form ("progn", SF_PROGN));
  obarray_put (o, special_form ("set!", SF_SETQ));
  obarray_put (o, special_form ("unwind-protect", SF_UNWIND_PROTECT));
  obarray_put (o, special_form ("condition-case", SF_CONDITION_CASE));
  obarray_put (o, q_quasiquote);
  obarray_put (o, q_unquote);
  obarray_put (o, q_unquote_splicing);
  obarray_put (o, q_error_arith);
  obarray_put (o, q_error_file);
  obarray_put (o, q_error_funcargs);
  obarray_put (o, q_error_funcinvalid);
  obarray_put (o, q_error_maxhandlerdepth);
  obarray_put (o, q_error_portclosed);
  obarray_put (o, q_error_portinvalid);
  obarray_put (o, q_error_portio);
  obarray_put (o, q_error_stackoverflow);
  obarray_put (o, q_error_syntax);
  obarray_put (o, q_error_type);
  obarray_put (o, q_error_unbound);
  obarray_put (o, q_error_unimplemented);

  q_cons = obarray_put (o, DEFSUBR ("cons", 2, 2, f_cons));
  obarray_put (o, DEFSUBR ("append", 2, 2, f_append));
  obarray_put (o, DEFSUBR ("setcar", 2, 2, f_setcar));
  obarray_put (o, DEFSUBR ("setcdr", 2, 2, f_setcdr));
  obarray_put (o, DEFSUBR ("vector", 1, 1, f_vector));
  obarray_put (o, DEFSUBR ("symbol", 1, 1, f_symbol));
  obarray_put (o, DEFSUBR ("symbol-value", 1, 1, f_symbol_value));
  obarray_put (o, DEFSUBR ("car", 1, 1, f_car));
  obarray_put (o, DEFSUBR ("cdr", 1, 1, f_cdr));
  obarray_put (o, DEFSUBR ("cadr", 1, 1, f_cadr));
  obarray_put (o, DEFSUBR ("cddr", 1, 1, f_cddr));
  obarray_put (o, DEFSUBR ("eq?", 2, 2, f_eq_p));
  obarray_put (o, DEFSUBR ("equal?", 2, 2, f_equal_p));
  obarray_put (o, DEFSUBR ("eqv?", 2, 2, f_eqv_p));
  obarray_put (o, DEFSUBR ("number?", 1, 1, f_number_p));
  obarray_put (o, DEFSUBR ("list?", 1, 1, f_list_p));
  obarray_put (o, DEFSUBR ("cons?", 1, 1, f_cons_p));
  obarray_put (o, DEFSUBR ("symbol?", 1, 1, f_symbol_p));
  obarray_put (o, DEFSUBR ("string?", 1, 1, f_string_p));
  obarray_put (o, DEFSUBR ("vector?", 1, 1, f_vector_p));
  obarray_put (o, DEFSUBR ("char?", 1, 1, f_char_p));
  obarray_put (o, DEFSUBR ("nil?", 1, 1, f_nil_p));
  obarray_put (o, DEFSUBR ("eval-noexpand", 1, 1, f_eval_noexpand));
  obarray_put (o, DEFSUBR ("apply", 2, 2, f_apply));
  obarray_put (o, DEFSUBR ("assoc", 2, 2, f_assoc));
  obarray_put (o, DEFSUBR ("assq", 2, 2, f_assq));
  obarray_put (o, DEFSUBR ("rassoc", 2, 2, f_rassoc));
  obarray_put (o, DEFSUBR ("rassq", 2, 2, f_rassq));
  obarray_put (o, DEFSUBR ("memq", 2, 2, f_memq));
  obarray_put (o, DEFSUBR ("member", 2, 2, f_member));
  obarray_put (o, DEFSUBR ("string=?", 2, 2, f_string_equal_p));
  obarray_put (o, DEFSUBR ("string-length", 1, 1, f_string_length));
  obarray_put (o, DEFSUBR ("string-ref", 2, 2, f_string_ref));
  obarray_put (o, DEFSUBR ("number->string", 1, 1, f_number_to_string));
  obarray_put (o, DEFSUBR ("string->number", 1, 1, f_string_to_number));
  obarray_put (o, DEFSUBR ("concat", 0, MANY, f_concat));
  obarray_put (o, DEFSUBR ("symbol->string", 1, 1, f_symbol_to_string));
  obarray_put (o, DEFSUBR ("string->symbol", 1, 1, f_string_to_symbol));
  obarray_put (o, DEFSUBR ("length", 1, 1, f_length));
  obarray_put (o, DEFSUBR ("+", 0, MANY, f_sum));
  obarray_put (o, DEFSUBR ("-", 1, MANY, f_subtract));
  obarray_put (o, DEFSUBR ("*", 0, MANY, f_multiply));
  obarray_put (o, DEFSUBR ("/", 1, MANY, f_divide));
  obarray_put (o, DEFSUBR ("min", 1, MANY, f_min));
  obarray_put (o, DEFSUBR ("max", 1, MANY, f_max));
  obarray_put (o, DEFSUBR ("abs", 1, 1, f_abs));
  obarray_put (o, DEFSUBR ("=", 2, 2, f_num_eq));
  obarray_put (o, DEFSUBR (">", 2, 2, f_ge));
  obarray_put (o, DEFSUBR (">=", 2, 2, f_geq));
  obarray_put (o, DEFSUBR ("<", 2, 2, f_le));
  obarray_put (o, DEFSUBR ("<=", 2, 2, f_leq));
  obarray_put (o, DEFSUBR ("defined?", 1, 1, f_defined_p));
  obarray_put (o, DEFSUBR ("format", 2, MANY, f_format));
  obarray_put (o, DEFSUBR ("read", 1, 1, f_read));
  obarray_put (o, DEFSUBR ("signal", 1, 2, f_signal));
  obarray_put (o, DEFSUBR ("error-symbol", 1, 1, f_error_symbol));
  obarray_put (o, DEFSUBR ("error-backtrace", 1, 1, f_error_backtrace));
  obarray_put (o, DEFSUBR ("error-data", 1, 1, f_error_data));
  obarray_put (o, DEFSUBR ("intern", 1, 1, f_intern));
  obarray_put (o, DEFSUBR ("gc", 0, 1, f_gc));
  obarray_put (o, DEFSUBR ("memstats", 0, 0, f_memstats));
  obarray_put (o, DEFSUBR ("memdump", 0, 0, f_memdump));
  obarray_put (o, DEFSUBR ("input-port?", 1, 1, f_input_port_p));
  obarray_put (o, DEFSUBR ("output-port?", 1, 1, f_output_port_p));
  obarray_put (o, DEFSUBR ("eof-object?", 1, 1, f_eof_object_p));
  obarray_put (o, DEFSUBR ("current-input-port", 0, 0, f_current_input_port));
  obarray_put (o,
               DEFSUBR ("current-output-port", 0, 0, f_current_output_port));
  obarray_put (
      o, DEFSUBR ("set-current-input-port!", 1, 1, f_set_current_input_port));
  obarray_put (o, DEFSUBR ("set-current-output-port!", 1, 1,
                           f_set_current_output_port));
  obarray_put (o, DEFSUBR ("open-input-file", 1, 1, f_open_input_file));
  obarray_put (o, DEFSUBR ("open-output-file", 1, 1, f_open_output_file));
  obarray_put (o, DEFSUBR ("open-input-string", 1, 1, f_open_input_string));
  obarray_put (o, DEFSUBR ("open-output-string", 0, 0, f_open_output_string));
  obarray_put (o, DEFSUBR ("get-output-string", 1, 1, f_get_output_string));
  obarray_put (o, DEFSUBR ("tcp-connect", 2, 2, f_tcp_connect));
  obarray_put (o, DEFSUBR ("unix-connect", 1, 1, f_unix_connect));
  obarray_put (o, DEFSUBR ("close-port", 1, 1, f_close_port));
  obarray_put (o, DEFSUBR ("read-char", 0, 1, f_read_char));
  obarray_put (o, DEFSUBR ("peek-char", 0, 1, f_peek_char));
  obarray_put (o, DEFSUBR ("char-ready?", 0, 1, f_char_ready_p));
  obarray_put (o, DEFSUBR ("write-char", 1, 2, f_write_char));
  obarray_put (o, DEFSUBR ("write", 1, 2, f_write));
  obarray_put (o, DEFSUBR ("display", 1, 2, f_display));
  obarray_put (o, DEFSUBR ("flush-output-port", 0, 1, f_flush_output_port));
}

// helpers impl

static Lisp_Object
assoc_w_pred (Lisp_Object key, Lisp_Object alist, lisp_2_pred keypred)
{
  Lisp_Object tail;
  for (tail = alist; !nil (tail); tail = f_cdr (tail))
    {
      Lisp_Object elt = f_car (tail);
      if (!consp (elt))
        // ignore it: emacs does this, other lisps error. i like this
        // because its flexible and easy to implement
        continue;
      if (keypred (f_car (elt), key) != q_nil)
        return elt;
    }
  return q_nil;
}

static Lisp_Object
rassoc_w_pred (Lisp_Object key, Lisp_Object alist, lisp_2_pred keypred)
{
  Lisp_Object tail;
  for (tail = alist; !nil (tail); tail = f_cdr (tail))
    {
      Lisp_Object elt = f_car (tail);
      if (!consp (elt))
        // ignore it: emacs does this, other lisps error. i like this
        // because its flexible and easy to implement
        continue;
      if (keypred (f_cdr (elt), key) != q_nil)
        return elt;
    }
  return q_nil;
}

static Lisp_Object
member_w_pred (Lisp_Object elt, Lisp_Object list, lisp_2_pred eltpred)
{
  if (nil (list))
    return q_nil;
  check_type (list, LISP_CONS);
  Lisp_Object tail = list;
  while (!nil (tail))
    {
      if (!nil (eltpred (elt, f_car (tail))))
        return tail;
      tail = f_cdr (tail);
    }
  return q_nil;
}
