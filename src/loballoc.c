#include "loballoc.h"
#include "error.h"
#include "lisp.h"
#include <stddef.h>
#include <stdlib.h>

struct loballocator *
loballoc_init ()
{
  struct loballocator *loba = malloc (sizeof (struct loballocator));
  loba->size = 0;
  loba->numblk = 0;
  loba->lobblks = NULL;
  return loba;
}

void *
loballoc (struct loballocator *loba, size_t size)
{
  struct lobblk *blk = malloc (sizeof (struct lobblk) + size);
  blk->owner = loba;
  blk->allocsize = size;
  blk->gcmark = 0;
  blk->next = loba->lobblks;

  loba->size += size;
  loba->numblk++;
  loba->lobblks = blk;
  return blk->data;
}

int
lobgcmark (struct loballocator *loba, void *objptr)
{
  struct lobblk *blk
      = (struct lobblk *)((char *)objptr - offsetof (struct lobblk, data));
  if (blk->owner != loba)
    internal_error ("Object not owned by loballoc at address %p\n", objptr);
    
  if (blk->gcmark)
    return 0;
  blk->gcmark = 1;
  return 1;
}

void
lobgcsweep (struct loballocator *loba)
{
  struct lobblk *blk = loba->lobblks;
  struct lobblk *prev = NULL;
  while (blk != NULL)
    {
      if (blk->gcmark)
        {
          blk->gcmark = 0;
          prev = blk;
          blk = blk->next;
          continue;
        }
      loba->size -= blk->allocsize;
      loba->numblk--;
      
      struct lobblk *blktofree = blk;
      blk = blk->next;
      if (prev == NULL)
        loba->lobblks = blk;
      else
        prev->next = blk;
      free (blktofree);
    }
}

void
lobmemdump (UNUSED struct loballocator *loba)
{
  // TODO
}
