#include "prelude.h"
#include "alloc.h"
#include "error.h"
#include "eval.h"
#include "lisp.h"
#include "stack.h"

static const char macroexpand_code[] = {
#embed "macroexpand.erlisp" suffix(, 0)
};

static const char prelude_code[] = {
#embed "prelude.erlisp" suffix(, 0)
};

static Lisp_Object
read_eval (Lisp_Object port)
{
  Lisp_Object form;
  while (!eq (form = f_read (port), q_eof))
    eval_toplevel (form);
  return q_t;
}

static void
load_embedded (const char *name, const char *code)
{
  Lisp_Object port = make_input_string_port (make_string (code));
  stack_current_ref ()->keepalive = port; // save port from gc
  Lisp_Object res;
  if (!condition_case_1 (read_eval, port, &res))
    internal_error ("Unable to load erlisp %s: %s\n", name,
                    unbox_string (unbox_symbol (f_car (res))->name)->data);
  stack_current_ref ()->keepalive = q_nil;
  f_close_port (port);
}

void
init_prelude ()
{
  load_embedded ("macroexpand", macroexpand_code);
  load_embedded ("prelude", prelude_code);
}
