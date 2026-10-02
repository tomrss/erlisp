#include "blkalloc.h"
#include "error.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


static struct blkpage *
blkpagenew (blkallocator *blka)
{
  int numblk = blka->blkperpage;

  // aligend memory is crucial for page recognition from object ptr
  struct blkpage *page = aligned_alloc (PAGE_SIZE, PAGE_SIZE);

  page->owner = blka;
  page->numused = 0;
  page->numblk = numblk;
  /* page->numbmapwords = numwords; */
  /* page->allocbmap = calloc (numwords, sizeof (uint64_t)); */
  /* page->gcmarkbmap = calloc (numwords, sizeof (uint64_t)); */
  for (size_t i = 0; i < NUMWORDS; i++)
    {
      page->allocbmap[i] = 0;
      page->gcmarkbmap[i] = 0;
    }

  /* memset (page->data, 0, PAGE_SIZE - sizeof (struct blkpage)); */

  page->next = blka->pages;
  blka->pages = page;
  blka->numpages++;

  return page;
}

static int
blkpagealloc (struct blkpage *page)
{
  for (size_t wordpos = 0; wordpos < NUMWORDS; wordpos++)
    {
      if (page->allocbmap[wordpos] == UINT64_MAX)
        continue;
      size_t bitpos = __builtin_ctzll (~page->allocbmap[wordpos]);
      if (wordpos * WORDBITS + bitpos >= page->numblk)
        // bitmap outside range
        return -1;
      // set block marker bit setting it as occupied and return total offset
      page->allocbmap[wordpos] |= (1ULL << bitpos);
      page->numused++;
      return WORDBITS * wordpos + bitpos;
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
  for (size_t i = 0; i < NUMWORDS; i++)
    {
      // gc sweep is just this: new bitmap of allocated blocks is the
      // previous one excluded every gc marked block
      page->allocbmap[i] &= page->gcmarkbmap[i];

      // reset marks
      page->gcmarkbmap[i] = 0;

      live += __builtin_popcountll (page->allocbmap[i]);
    }

  page->numused = live;
  return live;
}

blkallocator *
blkalloc_init (size_t blksize)
{
  if (blksize < BLKMIN)
    internal_error ("Block size minimum is %d\n", BLKMIN);

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
    .pagecurrent = NULL,
    .numpages = 0,
    .numused = 0,
    .gcgenerations = 0,
  };
  return blka;
}

void *
blkalloc (blkallocator *blka)
{
  struct blkpage *page = blka->pages;
  int offset = -1;

  // find page and page offset of the first free element
  // TODO: probably some heuristics here would be nice!
  while (page != NULL)
    {
      offset = blkpagealloc (page);
      if (offset >= 0)
        // found free blk in page
        break;
      page = page->next;
    }

  if (offset < 0)
    {
      // no free page, get a fresh one
      page = blkpagenew (blka);
      offset = blkpagealloc (page);
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
  struct blkpage *page = blka->pages;
  size_t live = 0;
  while (page != NULL)
    {
      live += blkpagegcsweep (page);
      page = page->next;
    }

  size_t blkfreed = blka->numused - live;
  blka->numused = live;
  blka->pagecurrent = blka->pages;
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
