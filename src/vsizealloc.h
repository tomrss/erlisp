#ifndef VSIZEALLOC_H
#define VSIZEALLOC_H

#include <stdalign.h>
#include <stddef.h>

// TODO BLKALIGN and this should be one define probably in lisp.h
#define VSIZEBLKALIGN 8

struct vsizeblk
{
  struct vsizeheap *owner;
  struct vsizeblk *next;
  size_t allocsize;
  char gcmark;
  alignas (VSIZEBLKALIGN) char data[];
};

struct vsizeheap
{
  size_t heapsize;
  size_t numblk;
  struct vsizeblk *blklist;
};

struct vsizeheap *vsizeheap_init ();
void *vsizealloc (struct vsizeheap *heap, size_t size);
int vsizegcmark (struct vsizeheap *heap, void *objptr);
void vsizegcsweep (struct vsizeheap *heap);
void vsizememdump (struct vsizeheap *heap);

#endif /* VSIZEALLOC_H */
