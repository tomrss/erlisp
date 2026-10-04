#include "vsizealloc.h"
#include "error.h"
#include "lisp.h"
#include <stddef.h>
#include <stdlib.h>

struct vsizeheap *
vsizeheap_init ()
{
  struct vsizeheap *heap = malloc (sizeof (struct vsizeheap));
  heap->heapsize = 0;
  heap->numblk = 0;
  heap->blklist = NULL;
  return heap;
}

void *
vsizealloc (struct vsizeheap *heap, size_t size)
{
  struct vsizeblk *blk = malloc (sizeof (struct vsizeblk) + size);
  blk->owner = heap;
  blk->allocsize = size;
  blk->gcmark = 0;
  blk->next = heap->blklist;

  heap->heapsize += size;
  heap->numblk++;
  heap->blklist = blk;
  return blk->data;
}

int
vsizegcmark (struct vsizeheap *heap, void *objptr)
{
  struct vsizeblk *blk
      = (struct vsizeblk *)((char *)objptr - offsetof (struct vsizeblk, data));
  if (blk->owner != heap)
    internal_error ("Object not owned by vsizealloc at address %p\n", objptr);
    
  if (blk->gcmark)
    return 0;
  blk->gcmark = 1;
  return 1;
}

void
vsizegcsweep (struct vsizeheap *heap)
{
  struct vsizeblk *blk = heap->blklist;
  struct vsizeblk *prev = NULL;
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
      
      struct vsizeblk *blktofree = blk;
      blk = blk->next;
      if (prev == NULL)
        heap->blklist = blk;
      else
        prev->next = blk;
      free (blktofree);
    }
}

void
vsizememdump (UNUSED struct vsizeheap *heap)
{
  // TODO
}
