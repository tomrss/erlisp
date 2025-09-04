#include "blkalloc.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static struct blk *
blkpop (struct blk **blklistptr)
{
  if (!*blklistptr)
    return NULL;
  struct blk *blk = *blklistptr;
  *blklistptr = blk->next;
  blk->next = NULL;
  return blk;
}

static void
blkpush (struct blk **blklistptr, struct blk *blk)
{
  blk->next = *blklistptr;
  *blklistptr = blk;
};

blkallocator *
blkalloc_init (size_t blksize, int (*blk_free_pred) (void *ptr))
{
  blkallocator *blka = calloc (1, sizeof (blkallocator));
  *blka = (struct blkallocator){
    .blksize = blksize,
    // (total page size - size of page struct (blk) because the page total
    // page size holds the whole page struct, including the header) divide by
    // the size of each block
    .blkperpage = (PAGE_SIZE - sizeof (struct blk)) / blksize,
    .pages = NULL,
    .freelist = NULL,
    .usedlist = NULL,
    .numpages = 0,
    .numfree = 0,
    .numused = 0,
    .blk_free_pred = blk_free_pred,
    .gcgenerations = 0,
  };
  return blka;
}

void *
blkalloc (blkallocator *blka)
{
  if (blka->freelist)
    {
      // we have at least an item in the freelist, return that.

      // NOTE: this block is the ONLY path in which the client can get
      // allocated memory.
      struct blk *firstfree = blkpop (&blka->freelist);
      blkpush (&blka->usedlist, firstfree);
      blka->numfree--;
      blka->numused++;
      return firstfree->ptr;
    }

  // we have to create another page with all elments in freelist
  struct blk *page = malloc (PAGE_SIZE);
  memset (page, 0, PAGE_SIZE);
  ptrdiff_t pageoffset = offsetof (struct blk, ptr);
  page->ptr = (void *)((uintptr_t)page + pageoffset);
  page->next = blka->pages;
  blka->pages = page;
  blka->numpages++;

  // TODO rather ugly way to construct linked list
  struct blk *prev;
  struct blk *freeblk = NULL;
  for (ptrdiff_t offset = 0; offset < PAGE_SIZE - pageoffset - blka->blksize;
       offset += blka->blksize)
    {
      prev = freeblk;
      freeblk = malloc (sizeof (struct blk));
      freeblk->ptr = (void *)((uintptr_t)(page->ptr) + offset);
      if (prev != NULL)
        prev->next = freeblk;
      else
        blka->freelist = freeblk;
      blka->numfree++;
    }
  freeblk->next = NULL;

  // now another call to ourselves should get a non-empty freelist
  return blkalloc (blka);
}

blkgcstats
blkgc (blkallocator *blka)
{
  size_t blkwalked = 0;
  size_t blkfreed = blka->numfree;
  struct blk *blk = blka->usedlist;
  struct blk *lastnotfreed = NULL;
  struct blk *next;
  while (blk)
    {
      next = blk->next;
      if (blka->blk_free_pred (blk->ptr))
        {
          // pop from used list
          if (lastnotfreed)
            // link last not freed el in used list to the next one
            lastnotfreed->next = next;
          else
            // still at start of list, pop advancing pointer
            blkpop (&blka->usedlist);

          blka->numused--;

          // push back to freelist
          blkpush (&blka->freelist, blk);
          blka->numfree++;
        }
      else
        {
          lastnotfreed = blk;
        }
      blkwalked++;
      blk = next;
    }
  blkfreed = blka->numfree - blkfreed;
  blka->gcgenerations++;
  return (blkgcstats){ .blkwalked = blkwalked, .blkfreed = blkfreed };
}

void
blkwalk (blkallocator *blka, void (*blk_action) (void *ptr))
{
  struct blk *blk = blka->usedlist;
  while (blk)
    {
      blk_action (blk->ptr);
      blk = blk->next;
    }
}

void
blkmemdump (blkallocator *blka)
{
  {
    struct blk *blk = blka->freelist;
    if (!blk)
      {
        printf ("empty userlist ()\n");
        goto dumpused;
      }
    int i = 0;
    printf ("freelist:\n");
    printf (" %3s %-14s %-14s %-12s\n", "ind", "block", "next", "ptr");
    while (blk)
      {
        printf (" %3d %-14p %-14p %-12p\n", i, blk, blk->next, blk->ptr);
        i++;
        blk = blk->next;
      }
  }
dumpused:
  {
    struct blk *blk = blka->usedlist;
    if (!blk)
      {
        printf ("empty userlist ()\n");
        return;
      }
    int i = 0;
    printf ("used list:\n");
    printf (" %3s %-14s %-14s %-12s\n", "ind", "block", "next", "ptr");
    while (blk)
      {
        printf (" %3d %-14p %-14p %-12p\n", i, blk, blk->next, blk->ptr);
        i++;
        blk = blk->next;
      }
  }
}

blkmemstats
blkstats (blkallocator *blka)
{
  return (blkmemstats){
    .numpages = blka->numpages,
    .sizepages = blka->numpages * PAGE_SIZE,
    .numfree = blka->numfree,
    .sizefree = blka->numfree * blka->blksize,
    .numused = blka->numused,
    .sizeused = blka->numused * blka->blksize,
    .internalfreelistsize = blka->numfree * sizeof (struct blk),
    .internalusedlistsize = blka->numused * sizeof (struct blk),
    .gcgenerations = blka->gcgenerations,
  };
}
