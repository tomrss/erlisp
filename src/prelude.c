#include "prelude.h"
#include "alloc.h"
#include "error.h"
#include "eval.h"
#include "lisp.h"
#include "port.h"
#include "print.h"
#include "stack.h"

static const char macroexpand_code[] = {
#embed "macroexpand.erlisp" suffix(, 0)
};

static const char prelude_code[] = {
#embed "prelude.erlisp" suffix(, 0)
};

static Lisp_Object
read_sexp_or_die (const char *name, Lisp_Object port)
{
  Lisp_Object readform = f_cons (q_read, f_cons (port, q_nil));
  Lisp_Object res;
  if (eval (l_globalenv, readform, &res))
    return res;

  print_error (res);
  internal_error ("Unable to load erlisp %s\n", name);
  exit (1);
}

static Lisp_Object
eval_or_die (const char *name, Lisp_Object form)
{
  Lisp_Object res;
  if (eval_toplevel (form, &res))
    return res;

  print_error (res);
  internal_error ("Unable to load erlisp %s\n", name);
  exit (1);
}

static void
load_embedded (const char *name, const char *code)
{
  Lisp_Object port = make_input_string_port (make_string (code));
  gcprotect (port);

  Lisp_Object form;
  while (!eq ((form = read_sexp_or_die (name, port)), q_eof))
      eval_or_die (name, form);

  f_close_port (port);
  gcunprotect (port);
}

void
init_prelude ()
{
  load_embedded ("macroexpand", macroexpand_code);
  load_embedded ("prelude", prelude_code);
}
