#include "prelude.h"
#include "alloc.h"
#include "error.h"
#include "eval.h"
#include "lisp.h"
#include "stack.h"

static const char prelude_code[] = {
#embed "prelude.erlisp" suffix(, 0)
};

static Lisp_Object
read_eval (Lisp_Object port)
{
  Lisp_Object form;
  while (!eq (form = f_read (port), q_eof))
    f_eval (form);
  return q_t;
}

void
init_prelude ()
{
  Lisp_Object port = make_input_string_port (make_string (prelude_code));
  stack_current_ref ()->keepalive = port; // save port from gc
  Lisp_Object res;
  if (!condition_case_1 (read_eval, port, &res))
    internal_error ("Unable to load erlisp prelude: %s\n",
                    unbox_string (unbox_symbol (f_car (res))->name)->data);
  stack_current_ref ()->keepalive = q_nil;
  f_close_port (port);
}
