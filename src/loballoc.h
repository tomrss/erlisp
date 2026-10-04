#ifndef LOBALLOC_H
#define LOBALLOC_H

#include <stdalign.h>
#include <stddef.h>

// TODO BLKALIGN and this should be one define probably in lisp.h
#define LOBBLKALIGN 8

struct lobblk
{
  struct lobheap *owner;
  struct lobblk *next;
  size_t allocsize;
  char gcmark;
  alignas (LOBBLKALIGN) char data[];
};

struct lobheap
{
  size_t heapsize;
  size_t numblk;
  struct lobblk *lobblks;
};

struct lobheap *lobheap_init ();
void *loballoc (struct lobheap *heap, size_t size);
int lobgcmark (struct lobheap *heap, void *objptr);
void lobgcsweep (struct lobheap *heap);
void lobmemdump (struct lobheap *heap);

#endif /* LOBALLOC_H */
