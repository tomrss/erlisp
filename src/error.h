#ifndef ERROR_H
#define ERROR_H

#include "alloc.h"
#include "eval.h"
#include "lisp.h"
#include "stack.h"
#include <stdarg.h>
#include <stdio.h>

// simple inlined error helpers for avoiding boilerplate

static inline Lisp_Object
type_name_string (Lisp_Type t)
{
  return make_string (type_name (t));
}

NORETURN static inline void
type_error (Lisp_Object obj, Lisp_Type expected)
{
  Lisp_Object data = f_cons (obj, q_nil);
  data = f_cons (type_name_string (type_of (obj)), data);
  data = f_cons (type_name_string (expected), data);
  f_signal (q_error_type, data);
}

NORETURN static inline void
type_error_2 (Lisp_Object obj, Lisp_Type t1, Lisp_Type t2)
{
  Lisp_Object data = f_cons (obj, q_nil);
  data = f_cons (type_name_string (type_of (obj)), data);
  data = f_cons (type_name_string (t2), data);
  data = f_cons (type_name_string (t1), data);
  f_signal (q_error_type, data);
}

NORETURN static inline void
file_error (Lisp_Object path)
{
  f_signal (q_error_file, f_cons (path, q_nil));
}

NORETURN static inline void
unimplemented_error (const char *msg)
{
  f_signal (q_error_unimplemented, f_cons (make_string (msg), q_nil));
}

NORETURN static inline void
funcargs_error (int minargs, int maxargs, int provided)
{
  Lisp_Object data = f_cons (box_int (provided), q_nil);
  data = f_cons (box_int (maxargs), data);
  data = f_cons (box_int (minargs), data);
  f_signal (q_error_funcargs, data);
}

NORETURN static inline void
unbound_error (Lisp_Object symbol)
{
  f_signal (q_error_unbound, f_cons (symbol, q_nil));
}

NORETURN static inline void
stackoverflow_error ()
{
  f_signal (q_error_stackoverflow, f_cons (box_int (STACKSIZE), q_nil));
}

NORETURN static inline void
arith_error (const char *msg)
{
  f_signal (q_error_arith, f_cons (make_string (msg), q_nil));
}

NORETURN static inline void
arith_error_2 (const char *msg, Lisp_Object details)
{
  Lisp_Object data = f_cons (details, q_nil);
  data = f_cons (make_string (msg), q_nil);
  f_signal (q_error_arith, data);
}

NORETURN static inline void
invalidfunc_error (Lisp_Object invalidfunc)
{
  f_signal (q_error_funcinvalid, f_cons (invalidfunc, q_nil));
}

static inline const char *
port_type_name (Lisp_Port_Type t)
{
  switch (t)
    {
    case PORT_FILE:
      return "PORT_FILE";
    case PORT_STRING:
      return "PORT_STRING";
    }
  return "UNKNONW";
}

static inline const char *
port_direction_name (Lisp_Port_Direction d)
{
  switch (d)
    {
    case PORT_IN:
      return "PORT_IN";
    case PORT_OUT:
      return "PORT_OUT";
    default:
      return "UNKNONW";
    }
}

NORETURN static inline void
invalidporttype_error (Lisp_Object port, Lisp_Port_Type expected)
{
  Lisp_Object data = f_cons (make_string (port_type_name (expected)), q_nil);
  data = f_cons (make_string (port_type_name (unbox_port (port)->type)), data);

  f_signal (q_error_portinvalid, data);
}

NORETURN static inline void
invalidportdir_error (Lisp_Object port, Lisp_Port_Direction expected)
{
  Lisp_Object data
      = f_cons (make_string (port_direction_name (expected)), q_nil);
  data = f_cons (
      make_string (port_direction_name (unbox_port (port)->direction)), data);

  f_signal (q_error_portinvalid, data);
}

NORETURN static inline void
closedport_error (Lisp_Object port)
{
  f_signal (q_error_portclosed, f_cons (port, q_nil));
}

NORETURN static inline void
syntax_error (const char *msg, int line)
{
  Lisp_Object data = f_cons (box_int (line), q_nil);
  data = f_cons (make_string (msg), data);
  f_signal (q_error_syntax, data);
}

static inline void
check_type (Lisp_Object obj, Lisp_Type type)
{
  if (type_of (obj) != type)
    type_error (obj, type);
}

NORETURN static inline void
internal_error (const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  vfprintf (stderr, fmt, args);
  va_end (args);
  abort ();
}

#endif /* ERROR_H */
