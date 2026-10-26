#include "print.h"
#include "lisp.h"
#include "port.h"
#include "stack.h"
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

void
port_printf (Lisp_Port *p, const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  char buf[256];
  int n = vsnprintf (buf, sizeof (buf), fmt, args);
  va_end (args);
  if (n < 0)
    return;
  if ((size_t)n >= sizeof (buf))
    n = sizeof (buf) - 1;

  port_write_nstring (buf, n, p);
}

static void
fprint_float (Lisp_Port *p, Lisp_Float f)
{
  if (isnan (f))
    {
      port_printf (p, "+nan.0");
      return;
    }
  if (isinf (f))
    {
      port_printf (p, f > 0 ? "+inf.0" : "-inf.0");
      return;
    }

  // shortest representation that reads back to the same value: 17
  // significant digits are always enough for a double
  char buf[32];
  for (int prec = 1; prec <= 17; prec++)
    {
      snprintf (buf, sizeof buf, "%.*g", prec, (double)f);
      if ((Lisp_Float)strtod (buf, NULL) == f)
        break;
    }

  // always look like a float, so that 1.0 is not printed as 1
  if (!strpbrk (buf, ".e"))
    strcat (buf, ".0");

  port_printf (p, "%s", buf);
}

// mirror of parse_char in lexer.c, so that a printed char reads back
static void
fprint_char (Lisp_Port *p, Lisp_Char c)
{
  switch (c)
    {
    case '\n':
      port_write_string ("?\\n", p);
      break;
    case '\t':
      port_write_string ("?\\t", p);
      break;
    case '\r':
      port_write_string ("?\\r", p);
      break;
    case ' ':
      port_write_string ("?\\s", p);
      break;
    case 27:
      port_write_string ("?\\e", p);
      break;
    case '\0':
      port_write_string ("?\\0", p);
      break;
    case '\\':
      port_write_string ("?\\\\", p);
      break;
    default:
      port_printf (p, "?%c", c);
    }
}

static void
fprint_list (Lisp_Port *p, Lisp_Object list, int display)
{
  port_write_char ('(', p);
  fprint_form (p, f_car (list), display);

  Lisp_Object tail = f_cdr (list);
  while (consp (tail))
    {
      port_write_char (' ', p);
      fprint_form (p, f_car (tail), display);
      tail = f_cdr (tail);
    }

  if (!nil (tail))
    {
      port_write_string (" . ", p);
      fprint_form (p, tail, display);
    }

  port_write_char (')', p);
}

void
print_form (Lisp_Object form, int display)
{
  fprint_form (unbox_port (f_current_output_port ()), form, display);
}

void
fprint_form (Lisp_Port *p, Lisp_Object form, int display)
{
  if (nil (form))
    {
      port_write_string ("nil", p);
      return;
    }
  if (eq (form, q_t))
    {
      port_write_char ('t', p);
      return;
    }
  if (unbound (form))
    {
      port_write_string ("UNBOUND", p);
      return;
    }

  switch (type_of (form))
    {
    case LISP_INTG:
      port_printf (p, "%ld", (long)unbox_int (form));
      break;
    case LISP_STRG:
      if (!display)
        port_write_char ('"', p);
      port_write_nstring (unbox_string (form)->data, unbox_string (form)->size,
                          p);
      if (!display)
        port_write_char ('"', p);
      break;
    case LISP_VECT:
      port_printf (p, "[size:%zu]", unbox_vector (form)->size);
      break;
    case LISP_PORT:
      port_printf (p, "[port,%s,%s%s]",
                   unbox_port (form)->direction == PORT_IN ? "in" : "out",
                   port_type_name (unbox_port (form)),
                   unbox_port (form)->closed ? ",closed" : "");
      break;
    case LISP_SUBR:
      port_printf (p, "['%s',%d,", unbox_subr (form)->name,
                   unbox_subr (form)->header.minargs);
      if (unbox_subr (form)->header.maxargs == MANY)
        port_printf (p, "many]");
      else
        port_printf (p, "%d]", unbox_subr (form)->header.maxargs);
      break;
    case LISP_LMBD:
      port_printf (p, "[lambda,%d,", unbox_lambda (form)->header.minargs);
      if (unbox_lambda (form)->header.maxargs == MANY)
        port_printf (p, "many]");
      else
        port_printf (p, "%d]", unbox_lambda (form)->header.maxargs);
      break;
    case LISP_SYMB:
      port_write_nstring (unbox_string (unbox_symbol (form)->name)->data,
                          unbox_string (unbox_symbol (form)->name)->size, p);
      break;
    case LISP_CONS:
      fprint_list (p, form, display);
      break;
    case LISP_FLOT:
      fprint_float (p, unbox_float (form));
      break;
    case LISP_CHAR:
      if (display)
        port_write_char (unbox_char (form), p);
      else
        fprint_char (p, unbox_char (form));
      break;
    case LISP_CONT:
      port_printf (p, "[continuation,%zu]", unbox_continuation (form)->size);
      break;
    }
}

void
print_error (Lisp_Object err)
{
  Lisp_Port *p = port_stderr ();

  fprint_form (p, f_error_symbol (err), 0);
  port_write_string (":", p);

  // data is usually a list of objects, e.g. (symbol) for unbound-error:
  // print its elements. anything else (e.g. an int) is printed as is
  Lisp_Object data = f_error_data (err);
  if (consp (data))
    for (Lisp_Object tail = data; consp (tail); tail = unbox_cons (tail)->cdr)
      {
        port_write_string (" ", p);
        fprint_form (p, unbox_cons (tail)->car, 0);
      }
  else if (!nil (data))
    {
      port_write_string (" ", p);
      fprint_form (p, data, 0);
    }
  port_write_string ("\n", p);

  Lisp_Object backtrace = f_error_backtrace (err);
  Lisp_Object tail = backtrace;

  port_write_string ("Backtrace:\n", p);
  while (!nil (tail))
    {
      const char *fname = unbox_string (f_car (tail))->data;
      port_printf (p, " at %s\n", fname);
      tail = f_cdr (tail);
    }
}
