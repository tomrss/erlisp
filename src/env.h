#ifndef ENV_H
#define ENV_H

#include "lisp.h"

Lisp_Object env_init ();
Lisp_Object env_new (Lisp_Object parent, Lisp_Object symbol);
Lisp_Object env_lookup (Lisp_Object env, Lisp_Object symbol);
Lisp_Object env_lookup_name (Lisp_Object env, Lisp_Object name);
Lisp_Object env_current ();

#endif /* ENV_H */
