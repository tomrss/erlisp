#ifndef BLKALLOC_H
#define BLKALLOC_H

#include "lisp.h"
#include <assert.h>
#include <stdalign.h>
#include <stddef.h>

#define PAGE_SHIFT 12 // 4096 byte (pageshift = log2(pagesize))
#define PAGE_SIZE ((uintptr_t)1 << PAGE_SHIFT) // just 2^pageshift
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define BLKMIN 16
#define BLKALIGN 8
#define WORDBITS 64 // n bits in uint64_t
#define NUMWORDS ((PAGE_SIZE / BLKMIN + WORDBITS - 1) / WORDBITS)

typedef struct blkallocator blkallocator;
typedef struct blkgcstats blkgcstats;
typedef struct blkmemstats blkmemstats;

struct blkpage
{
  struct blkallocator *owner;
  struct blkpage *next;
  struct blkpage *nextavail;
  size_t numblk;
  size_t numused;
  size_t numwords;
  size_t firstfreeword;
  uint64_t allocbmap[NUMWORDS];
  uint64_t gcmarkbmap[NUMWORDS];
  alignas (BLKALIGN) char data[];
};

static_assert (offsetof (struct blkpage, data) % BLKALIGN == 0,
               "Error: blkpage data should be 8 byte aligned");

struct blkallocator
{
  ptrdiff_t blksize;                // constant size of each block
  size_t blkperpage;                // number of blocks in each page
  struct blkpage *pages;            // linked list of pages
  struct blkpage *availpages;       // linked list of pages with free blocks
  size_t numpages;                  // number of allocated blck pages
  size_t numused;                   // number of used elements
  int (*blk_free_pred) (void *ptr); // tells when a blk can be freed
};

struct blkgcstats
{
  size_t blkwalked;
  size_t blkfreed;
};

struct blkmemstats
{
  unsigned long int numpages; // number of allocated blck pages
  size_t sizepages;           // bytes allocated in block pages
  unsigned long int numused;  // number of used elements
  size_t sizeused;            // size of used elements in bytes
};

blkallocator *blkalloc_init (size_t blksize);
void *blkalloc (blkallocator *blka);
blkgcstats blkgcsweep (blkallocator *blka);
int blkgcmark (blkallocator *blka, void *objptr);
blkmemstats blkstats (blkallocator *blka);
void blkmemdump (blkallocator *blka);

#endif /* BLKALLOC_H */
