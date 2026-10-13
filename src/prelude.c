#include "prelude.h"
#include "alloc.h"
#include "error.h"
#include "eval.h"
#include "lisp.h"
#include "stack.h"

static const char prelude_code[] = {
#embed "prelude.erlisp" suffix(, 0)
};

void
init_prelude ()
{
  Lisp_Object port = make_input_string_port (make_string (prelude_code));
  stack_current_ref ()->keepalive = port; // save port from gc
  Lisp_Object res;
  if (!condition_case_1 (f_load_from_port, port, &res))
    internal_error ("Unable to load erlisp prelude: %s\n",
                    unbox_string (unbox_symbol (f_car (res))->name)->data);
  stack_current_ref ()->keepalive = q_nil;
  f_close_port (port);
}
