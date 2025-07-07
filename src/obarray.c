#include "obarray.h"
#include "alloc.h"
#include "lisp.h"
#include <stdio.h>
#include <string.h>

static int hash_string (const char *s, size_t size);

Lisp_Object
obarray_init ()
{
  Lisp_Object obarray = make_vector (OBARRAY_BUCKETS);
  return obarray;
}

Lisp_Object
obarray_put (Lisp_Object obarray, Lisp_Object symbol)
{
  Lisp_Symbol *usymbol = unbox_symbol (symbol);
  Lisp_Vector *uobarray = unbox_vector (obarray);

  Lisp_Object found = obarray_lookup_name (obarray, usymbol->name);

  // the get function returns the bucket index in case of not found
  if (type_of (found) != LISP_INTG)
    // not an integer, this means: found!
    return found;

  // not found, put symbol in obarray
  int hash = unbox_int (found);

  Lisp_Object *ptr = &uobarray->contents[hash];
  if (*ptr)
    {
      // there are already elements in bucket. put new at beginning!
      usymbol->next = unbox_symbol (*ptr);
    }

  *ptr = box_symbol (usymbol);

  return symbol;
}

Lisp_Object
obarray_lookup (Lisp_Object obarray, Lisp_Object symbol)
{
  return obarray_lookup_name(obarray, unbox_symbol(symbol)->name);
}

Lisp_Object
obarray_lookup_name (Lisp_Object obarray, Lisp_Object name)
{
  Lisp_Vector *uobarray = unbox_vector (obarray);
  Lisp_String *uname = unbox_string (name);

  int hash = hash_string (uname->data, uname->size);
  Lisp_Symbol *unode = unbox_symbol (uobarray->contents[hash]);
  while (unode)
    {
      Lisp_String *unodename = unbox_string (unode->name);
      if (uname->size == unodename->size
          && strncmp (uname->data, unodename->data, uname->size) == 0)
        return box_symbol (unode);
      unode = unode->next;
    }

  // not found. in this case, we return the hash, i.e. the index of the
  // bucket in which the element would be saved if it only existed. this
  // allows us to immediately put it in the correct bucket in the put
  // function. trick taken from rms implementation of obarray in emacs 16
  return box_int (hash);
}

static int
hash_string (const char *s, size_t size)
{
  // standard djb2 implementation but for NOT null terminated string
  unsigned long hash = 5381;

  for (size_t i = 0; i < size; i++)
    hash = ((hash << 5) + hash) + s[i]; /* hash * 33 + c */

  return hash % OBARRAY_BUCKETS;
}
