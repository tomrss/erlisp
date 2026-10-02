#include "../src/blkalloc.h"
#include "test_lib.h"
#include <string.h>

// test cases
static TestResult test_blkalloc_alloc ();
static TestResult test_blkalloc_2pages ();
static TestResult test_blkalloc_gc ();
static TestResult test_blkalloc_gc_3pages ();

static TestCase test_blkalloc_cases[] = {
  { .skip = 0, .name = "alloc", .run = test_blkalloc_alloc },
  { .skip = 0, .name = "2pages", .run = test_blkalloc_2pages },
  { .skip = 0, .name = "gc", .run = test_blkalloc_gc },
  { .skip = 0, .name = "gc 3pages", .run = test_blkalloc_gc_3pages },
  {}, // terminator
};

struct cust
{
  char gcmark;
  size_t size;
  uint64_t data;
};

static int
test_free_cust (void *ptr)
{
  struct cust *s = ptr;
  return s->gcmark;
}

TestSuite *
test_suite_blkalloc ()
{
  return test_suite_init ("blkalloc", test_blkalloc_cases);
}

// test cases implementation

static size_t
blklist_size (struct blk *list)
{
  size_t i = 0;
  struct blk *blk = list;
  while (blk)
    {
      i++;
      blk = blk->next;
    }
  return i;
}

static size_t
blka_used_and_free (blkallocator *blka)
{
  size_t s1 = blklist_size (blka->freelist);
  size_t s2 = blklist_size (blka->usedlist);
  return s1 + s2;
}

static TestResult
test_blkalloc_alloc ()
{
  blkallocator *blka = blkalloc_init (sizeof (struct cust), test_free_cust);
  size_t pageblknum
      = (PAGE_SIZE - offsetof (struct blk, ptr) - 1) / sizeof (struct cust);
  struct cust *s = blkalloc (blka);
  TEST_ASSERT (blka->pages->ptr == s,
               "expected first alloc at start of page. page: %p, firstobj: %p",
               blka->pages->ptr, s);
  s->data = 42;
  s->size = 4;
  TEST_ASSERT (s->size == 4, "wrong stored size: %ld", s->size);

  size_t freelistsize = blklist_size (blka->freelist);
  TEST_ASSERT (freelistsize == pageblknum - 1,
               "free list size: expected %d, got %d", pageblknum - 1,
               freelistsize);
  for (size_t i = 0; i < pageblknum - 1; i++)
    {
      s = blkalloc (blka);
      freelistsize = blklist_size (blka->freelist);
      TEST_ASSERT (freelistsize == pageblknum - i - 2,
                   "free list size: expected %d, got %d", pageblknum - i - 2,
                   freelistsize);
    }
  TEST_ASSERT (freelistsize == 0, "free list size: expected %d, got %d", 0,
               freelistsize);
  return TEST_RESULT_SUCCESS;
}

static TestResult
test_blkalloc_2pages ()
{
  blkallocator *blka = blkalloc_init (sizeof (struct cust), test_free_cust);
  size_t pageblknum
      = (PAGE_SIZE - offsetof (struct blk, ptr) -1 ) / sizeof (struct cust);

  size_t freelistsize;
  size_t pagesize;
  size_t usedsize;
  for (size_t i = 0; i < pageblknum; i++)
    {
      blkalloc (blka);
      freelistsize = blklist_size (blka->freelist);
      usedsize = blklist_size (blka->usedlist);
      pagesize = blklist_size (blka->pages);
      TEST_ASSERT (freelistsize == pageblknum - i - 1,
                   "free list size: expected %d, got %d", pageblknum - i - 1,
                   freelistsize);
      TEST_ASSERT (usedsize == i + 1, "used list size: expected %d, got %d",
                   i + 1, freelistsize);
      TEST_ASSERT (pagesize == 1, "page size: expected %d, got %d", 1,
                   pagesize);
    }

  blkalloc (blka);
  freelistsize = blklist_size (blka->freelist);
  size_t newusedsize = blklist_size (blka->usedlist);
  pagesize = blklist_size (blka->pages);

  TEST_ASSERT (freelistsize == pageblknum - 1,
               "free list size: expected %d, got %d", pageblknum - 1,
               freelistsize);
  TEST_ASSERT (newusedsize == usedsize + 1,
               "used list size: expected %d, got %d", usedsize + 1,
               newusedsize);
  TEST_ASSERT (pagesize == 2, "page size: expected %d, got %d", 2, pagesize);

  return TEST_RESULT_SUCCESS;
}

/* static void */
/* printf_blklist (struct blk *list) */
/* { */
/*   int i = 0; */
/*   struct blk *blk = list; */
/*   while (blk) */
/*     { */
/*       struct cust *s = blk->ptr; */
/*       printf ("%4d: blk %15p, next %15p, ptr %12p, gc %d, val %2llu\n", i,
 * blk, */
/*               blk->next, blk->ptr, s->gcmark, s->data); */
/*       i++; */
/*       blk = blk->next; */
/*     } */
/* } */

static TestResult
test_blkalloc_gc ()
{
  blkallocator *blka = blkalloc_init (sizeof (struct cust), test_free_cust);
  TEST_ASSERT (blklist_size (blka->pages) == 0, "init page size !=0");
  TEST_ASSERT (blklist_size (blka->freelist) == 0, "init freelist size !=0");
  TEST_ASSERT (blklist_size (blka->usedlist) == 0, "init usedlist size !=0");

  size_t pageblknum
      = (PAGE_SIZE - offsetof (struct blk, ptr) -1) / sizeof (struct cust);

  // assume 20 > pageblknum
  size_t N = 20;

  for (size_t i = 0; i < N; i++)
    {
      struct cust *s = blkalloc (blka);

      s->data = i;
      s->size = 3;
      s->gcmark = i % 2;
      TEST_ASSERT (blka_used_and_free (blka) == pageblknum,
                   "used + free invariant: exp %d, got %d", pageblknum,
                   blka_used_and_free (blka));
    }

  struct blkgcstats gcstats = blkgc (blka);

  TEST_ASSERT (gcstats.blkwalked == N, "gc blk walked: exp %d, got %d", N,
               gcstats.blkwalked);
  TEST_ASSERT (gcstats.blkfreed == N/2, "gc blk freed: exp %d, got %d", N/2,
               gcstats.blkfreed);

  TEST_ASSERT (blka_used_and_free (blka) == pageblknum,
               "used + free invariant: exp %d, got %d", pageblknum,
               blka_used_and_free (blka));
  TEST_ASSERT (blklist_size (blka->usedlist) == N / 2,
               "used list size: exp %d, got %d", N / 2,
               blklist_size (blka->usedlist));

  return TEST_RESULT_SUCCESS;
}

static TestResult
test_blkalloc_gc_3pages ()
{
  blkallocator *blka = blkalloc_init (sizeof (struct cust), test_free_cust);
  TEST_ASSERT (blklist_size (blka->pages) == 0, "init page size !=0");
  TEST_ASSERT (blklist_size (blka->freelist) == 0, "init freelist size !=0");
  TEST_ASSERT (blklist_size (blka->usedlist) == 0, "init usedlist size !=0");

  size_t pageblknum
      = (PAGE_SIZE - offsetof (struct blk, ptr) -1) / sizeof (struct cust);

  // assume 20 > pageblknum
  size_t N = pageblknum * 2 + 10;

  for (size_t i = 0; i < N; i++)
    {
      struct cust *s = blkalloc (blka);

      s->data = i;
      s->size = 3;
      s->gcmark = i % 2;
      int npag = i / pageblknum + 1;
      TEST_ASSERT (blka_used_and_free (blka) == npag * pageblknum,
                   "used + free invariant: exp %zu, got %zu\n",
                   npag * pageblknum, blka_used_and_free (blka));
    }

  blkgc (blka);

  TEST_ASSERT (blka_used_and_free (blka) == 3 * pageblknum,
               "used + free invariant: exp %d, got %d", 3 * pageblknum,
               blka_used_and_free (blka));

  TEST_ASSERT (blklist_size (blka->usedlist) == N / 2,
               "used list size: exp %d, got %d", N / 2,
               blklist_size (blka->usedlist));

  return TEST_RESULT_SUCCESS;
}
