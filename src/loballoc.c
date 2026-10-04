#include "loballoc.h"
#include "error.h"
#include "lisp.h"
#include <stddef.h>
#include <stdlib.h>

struct lobheap *
lobheap_init ()
{
  struct lobheap *heap = malloc (sizeof (struct lobheap));
  heap->heapsize = 0;
  heap->numblk = 0;
  heap->lobblks = NULL;
  return heap;
}

void *
loballoc (struct lobheap *heap, size_t size)
{
  struct lobblk *blk = malloc (sizeof (struct lobblk) + size);
  blk->owner = heap;
  blk->allocsize = size;
  blk->gcmark = 0;
  blk->next = heap->lobblks;

  heap->heapsize += size;
  heap->numblk++;
  heap->lobblks = blk;
  return blk->data;
}

int
lobgcmark (struct lobheap *heap, void *objptr)
{
  struct lobblk *blk
      = (struct lobblk *)((char *)objptr - offsetof (struct lobblk, data));
  if (blk->owner != heap)
    internal_error ("Object not owned by loballoc at address %p\n", objptr);
    
  if (blk->gcmark)
    return 0;
  blk->gcmark = 1;
  return 1;
}

void
lobgcsweep (struct lobheap *heap)
{
  struct lobblk *blk = heap->lobblks;
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
      heap->heapsize -= blk->allocsize;
      heap->numblk--;
      
      struct lobblk *blktofree = blk;
      blk = blk->next;
      if (prev == NULL)
        heap->lobblks = blk;
      else
        prev->next = blk;
      free (blktofree);
    }
}

void
lobmemdump (UNUSED struct lobheap *heap)
{
  // TODO
}
