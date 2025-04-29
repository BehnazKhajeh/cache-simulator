/*
 * cache.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include "cache.h"
#include "main.h"

/* cache configuration parameters */
static int cache_split = 0;
static int cache_usize = DEFAULT_CACHE_SIZE;
static int cache_isize = DEFAULT_CACHE_SIZE;
static int cache_dsize = DEFAULT_CACHE_SIZE;
static int cache_block_size = DEFAULT_CACHE_BLOCK_SIZE;
static int words_per_block = DEFAULT_CACHE_BLOCK_SIZE / WORD_SIZE;
static int cache_assoc = DEFAULT_CACHE_ASSOC;
static int cache_writeback = DEFAULT_CACHE_WRITEBACK;
static int cache_writealloc = DEFAULT_CACHE_WRITEALLOC;

/* cache model data structures */
static cache c1;
static cache_stat cache_stat_inst;
static cache_stat cache_stat_data;

/************************************************************/
void set_cache_param(int param, int value)
{
  switch (param) {
  case CACHE_PARAM_BLOCK_SIZE:
    cache_block_size = value;
    words_per_block = value / WORD_SIZE;
    break;
  case CACHE_PARAM_USIZE:
    cache_split = FALSE;
    cache_usize = value;
    break;
  case CACHE_PARAM_ISIZE:
    cache_split = TRUE;
    cache_isize = value;
    break;
  case CACHE_PARAM_DSIZE:
    cache_split = TRUE;
    cache_dsize = value;
    break;
  case CACHE_PARAM_ASSOC:
    cache_assoc = value;
    break;
  case CACHE_PARAM_WRITEBACK:
    cache_writeback = TRUE;
    break;
  case CACHE_PARAM_WRITETHROUGH:
    cache_writeback = FALSE;
    break;
  case CACHE_PARAM_WRITEALLOC:
    cache_writealloc = TRUE;
    break;
  case CACHE_PARAM_NOWRITEALLOC:
    cache_writealloc = FALSE;
    break;
  default:
    printf("error set_cache_param: bad parameter value\n");
    exit(-1);
  }
}
/************************************************************/
void init_cache()
{
  int index_ = cache_usize / cache_block_size;
  c1.associativity = cache_assoc;
  c1.n_sets = index_ / cache_assoc;
  c1.index_mask = c1.n_sets - 1;

  c1.LRU_head = (Pcache_line *)malloc(c1.n_sets * sizeof(Pcache_line));
  for (int i = 0; i < c1.n_sets; i++) {
    c1.LRU_head[i] = (cache_line *)malloc(c1.associativity * sizeof(cache_line));
    for (int j = 0; j < c1.associativity; j++) {
      c1.LRU_head[i][j].tag = 0;
      c1.LRU_head[i][j].dirty = 0;
      c1.LRU_head[i][j].LRU_next = NULL;
      c1.LRU_head[i][j].LRU_prev = NULL;
    }
  }
}
/************************************************************/
void perform_access(unsigned addr, unsigned access_type)
{
  int b = log2(cache_block_size);
  int s = log2(c1.n_sets);
  unsigned tag = addr >> (s + b);
  unsigned index = (addr >> b) & c1.index_mask;

  bool hit = false;
  int hit_index = -1;

  for (int j = 0; j < c1.associativity; j++) {
    if (c1.LRU_head[index][j].tag == tag) {
      hit = true;
      hit_index = j;
      break;
    }
  }

  cache_stat *stat = (access_type == 2) ? &cache_stat_inst : &cache_stat_data;

  stat->accesses++;

  if (hit) {
    stat->demand_fetches++;
    if (access_type != 2 && cache_writeback) {
      c1.LRU_head[index][hit_index].dirty = 1;
    }
  } else {
    stat->misses++;
    stat->demand_fetches++;
    if (cache_writealloc || access_type == 2) {
      c1.LRU_head[index][0].tag = tag;
      c1.LRU_head[index][0].dirty = 0;
    }
  }
}
/************************************************************/
void flush()
{
  for (int i = 0; i < c1.n_sets; i++) {
    for (int j = 0; j < c1.associativity; j++) {
      if (c1.LRU_head[i][j].dirty) {
        c1.LRU_head[i][j].dirty = 0;
      }
    }
    free(c1.LRU_head[i]);
  }
  free(c1.LRU_head);
}
/************************************************************/
void delete(Pcache_line *head, Pcache_line *tail, Pcache_line item)
{
  if (item->LRU_prev) {
    item->LRU_prev->LRU_next = item->LRU_next;
  } else {
    *head = item->LRU_next;
  }

  if (item->LRU_next) {
    item->LRU_next->LRU_prev = item->LRU_prev;
  } else {
    *tail = item->LRU_prev;
  }
}
/************************************************************/
void insert(Pcache_line *head, Pcache_line *tail, Pcache_line item)
{
  item->LRU_next = *head;
  item->LRU_prev = NULL;

  if (*head)
    (*head)->LRU_prev = item;
  else
    *tail = item;

  *head = item;
}
/************************************************************/
void dump_settings()
{
  printf("*** CACHE SETTINGS ***\n");
  if (cache_split) {
    printf("  Split I/D Caches:\n");
    printf("    I-Cache Size: %d\n", cache_isize);
    printf("    D-Cache Size: %d\n", cache_dsize);
  } else {
    printf("  Unified Cache Size: %d\n", cache_usize);
  }
  printf("  Associativity: %d\n", cache_assoc);
  printf("  Block Size: %d\n", cache_block_size);
  printf("  Write Policy: %s\n", cache_writeback ? "Write Back" : "Write Through");
  printf("  Allocation Policy: %s\n", cache_writealloc ? "Write Allocate" : "No Write Allocate");
}
/************************************************************/
