#ifndef BLKALLOC_H
#define BLKALLOC_H

#include "lisp.h"
#include <stddef.h>

#define PAGE_SIZE 2048

typedef struct blkallocator blkallocator;
typedef struct blkgcstats blkgcstats;
typedef struct blkmemstats blkmemstats;

struct blk
{
  struct blk *next;
  void *ptr;
};

struct blkallocator
{
  ptrdiff_t blksize;                // constant size of each block
  size_t blkperpage;                // number of blocks in each page
  struct blk *pages;                // linked list of pages
  struct blk *freelist;             // linked list of free blocks (freelist)
  struct blk *usedlist;             // linked list of used blocks
  size_t numpages;                  // number of allocated blck pages
  size_t numfree;                   // number of elements in freelist
  size_t numused;                   // number of used elements
  int (*blk_free_pred) (void *ptr); // tells when a blk can be freed
  unsigned int gcgenerations;       // count of gc runs
};

struct blkgcstats
{
  size_t blkwalked;
  size_t blkfreed;
};

struct blkmemstats
{
  unsigned long int numpages;  // number of allocated blck pages
  size_t sizepages;            // bytes allocated in block pages
  unsigned long int numfree;   // number of elements in freelist
  size_t sizefree;             // size of freelist in bytes
  unsigned long int numused;   // number of used elements
  size_t sizeused;             // size of used elements in bytes
  size_t internalfreelistsize; // size of the freelist internal representation
  size_t internalusedlistsize; // size of the usedlist internal representation
  unsigned int gcgenerations;  // count of gc runs
};

blkallocator *blkalloc_init (size_t blksize, int (*blk_free_pred) (void *ptr));
void *blkalloc (blkallocator *blka);
void blkwalk (blkallocator *blka, void (*blk_action) (void *ptr));
blkgcstats blkgc (blkallocator *blka);
blkmemstats blkstats (blkallocator *blka);
void blkmemdump (blkallocator *blka);

#endif /* BLKALLOC_H */
