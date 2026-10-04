#include "blkalloc.h"
#include "error.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static struct blkpage *
blkpagenew (blkallocator *blka)
{
  size_t numblk = blka->blkperpage;
  size_t numwords = (numblk + WORDBITS - 1) / WORDBITS;

  // aligend memory is crucial for page recognition from object ptr
  struct blkpage *page = aligned_alloc (PAGE_SIZE, PAGE_SIZE);

  page->owner = blka;
  page->numused = 0;
  page->numblk = numblk;
  page->firstfreeword = 0;
  page->numwords = numwords;
  for (size_t i = 0; i < numwords; i++)
    {
      page->allocbmap[i] = 0;
      page->gcmarkbmap[i] = 0;
    }

  page->next = blka->pages;
  blka->pages = page;
  blka->numpages++;

  return page;
}

static int
blkpagealloc (struct blkpage *page)
{
  for (size_t i = page->firstfreeword; i < page->numwords; i++)
    {
      if (page->allocbmap[i] == UINT64_MAX)
        {
          page->firstfreeword++;
          continue;
        }

      size_t bitpos = __builtin_ctzll (~page->allocbmap[i]);
      if (i * WORDBITS + bitpos >= page->numblk)
        // bitmap outside range
        return -1;
      // set block marker bit setting it as occupied and return total offset
      page->allocbmap[i] |= (1ULL << bitpos);
      page->numused++;
      return WORDBITS * i + bitpos;
    }

  return -1;
}

static int
blkpagegcmark (struct blkpage *page, size_t blkpos)
{
  size_t wordpos = blkpos / 64;
  size_t bitpos = blkpos % 64;

  // set blok gc marker bit setting it as marked
  uint64_t mask = 1ULL << bitpos;
  uint64_t old = page->gcmarkbmap[wordpos];

  page->gcmarkbmap[wordpos] = old | mask;
  return !(old & mask);
}

static size_t
blkpagegcsweep (struct blkpage *page)
{
  size_t live = 0;
  for (size_t i = 0; i < page->numwords; i++)
    {
      // gc sweep is just this: new bitmap of allocated blocks is the
      // previous one excluded every gc marked block
      page->allocbmap[i] &= page->gcmarkbmap[i];

      // reset marks
      page->gcmarkbmap[i] = 0;

      live += __builtin_popcountll (page->allocbmap[i]);
    }

  page->numused = live;
  page->firstfreeword = 0;
  return live;
}

blkallocator *
blkalloc_init (size_t blksize)
{
  if (blksize < BLKMIN)
    internal_error ("Block size minimum is %d\n", BLKMIN);

  // round up to multiple of BLKALIGN for respecting memory alignment
  blksize = blksize % BLKALIGN == 0
                ? blksize
                : (blksize / BLKALIGN) * BLKALIGN + BLKALIGN;

  // TODO why calloc instead of malloc? i don't remember and cannot find a good
  // reason right now.
  blkallocator *blka = calloc (1, sizeof (blkallocator));
  *blka = (struct blkallocator){
    .blksize = blksize,
    // (total page size - size of page struct (blk) because the page total
    // page size holds the whole page struct, including the header) divide by
    // the size of each block
    .blkperpage = (PAGE_SIZE - sizeof (struct blkpage)) / blksize,
    .pages = NULL,
    .availpages = NULL,
    .numpages = 0,
    .numused = 0,
    .gcgenerations = 0,
  };
  return blka;
}

void *
blkalloc (blkallocator *blka)
{
  struct blkpage *page = blka->availpages;
  int offset = -1;

  // find page and page offset of the first free element
  while (page != NULL)
    {
      offset = blkpagealloc (page);
      if (offset >= 0)
        // found free blk in page
        break;
      // page is full, remove it from availpages
      blka->availpages = page->nextavail;
      page = page->nextavail;
    }

  if (offset < 0)
    {
      // no free page, get a fresh one
      page = blkpagenew (blka);
      offset = blkpagealloc (page);
      page->nextavail = blka->availpages;
      blka->availpages = page;
    }

  blka->numused++;

  return page->data + offset * blka->blksize;
}

int
blkgcmark (blkallocator *blka, void *objptr)
{
  // get page from the ptr address: the page is aligned so we can do this
  struct blkpage *page = (struct blkpage *)((uintptr_t)objptr & PAGE_MASK);
  if (page->owner != blka)
    internal_error ("Object not owned by blkalloc at address %p\n", objptr);

  size_t pageoffset = ((char *)objptr - page->data) / blka->blksize;

  return blkpagegcmark (page, pageoffset);
}

blkgcstats
blkgcsweep (blkallocator *blka)
{
  blka->availpages = NULL; // will be rebuilt from scratch

  struct blkpage *page = blka->pages;
  size_t live = 0;
  while (page != NULL)
    {
      size_t pagelive = blkpagegcsweep (page);

      if (pagelive < page->numblk)
        {
          // TODO some heuristics here on sorting of pages could help
          // in containing memory fragmentation
          page->nextavail = blka->availpages;
          blka->availpages = page;
        }
      else
        page->nextavail = NULL;

      live += pagelive;
      page = page->next;
    }

  size_t blkfreed = blka->numused - live;
  blka->numused = live;
  blka->gcgenerations++;
  return (blkgcstats){ .blkwalked = blka->numused, .blkfreed = blkfreed };
}

void
blkmemdump (UNUSED blkallocator *blka)
{
  // not impl
}

blkmemstats
blkstats (blkallocator *blka)
{
  return (blkmemstats){
    .numpages = blka->numpages,
    .sizepages = blka->numpages * PAGE_SIZE,
    .numused = blka->numused,
    .sizeused = blka->numused * blka->blksize,
    .gcgenerations = blka->gcgenerations,
  };
}
