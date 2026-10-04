#ifndef LOBALLOC_H
#define LOBALLOC_H

#include <stdalign.h>
#include <stddef.h>

// TODO BLKALIGN and this should be one define probably in lisp.h
#define LOBBLKALIGN 8

typedef struct loballocator loballocator;

struct lobblk
{
  struct loballocator *owner;
  struct lobblk *next;
  size_t allocsize;
  char gcmark;
  alignas (LOBBLKALIGN) char data[];
};

struct loballocator
{
  size_t size;
  size_t numblk;
  struct lobblk *lobblks;
};

struct loballocator *loballoc_init ();
void *loballoc (struct loballocator *loba, size_t size);
int lobgcmark (struct loballocator *loba, void *objptr);
void lobgcsweep (struct loballocator *loba);
void lobmemdump (struct loballocator *loba);

#endif /* LOBALLOC_H */
