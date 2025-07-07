#ifndef OBARRAY_H
#define OBARRAY_H

/*
  The obarray (object array) is a unordered data structure that
  contains symbols and allows to lookup them.

  It is a hash table implemented as follows:
  - obarray is a lisp vector, each element represents
 */

#include "lisp.h"

#define OBARRAY_BUCKETS 512

Lisp_Object obarray_init();
Lisp_Object obarray_put(Lisp_Object obarray, Lisp_Object symbol);
Lisp_Object obarray_lookup(Lisp_Object obarray, Lisp_Object symbol);
Lisp_Object obarray_lookup_name(Lisp_Object obarray, Lisp_Object name);

#endif /* OBARRAY_H */
