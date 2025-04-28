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
static Pcache icache;
static Pcache dcache;
static cache c1;
static cache c2;
static cache_stat cache_stat_inst;
static cache_stat cache_stat_data;
static cache_line* next;
static cache_line* currline;
static cache_line* head;

/************************************************************/
void set_cache_param(int param, int value) {
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
void init_cache() {
  /* initialize the cache, and cache statistics data structures */
  int index_ = cache_usize / cache_block_size;

  //cache c1
  c1.associativity = cache_assoc;
  c1.n_sets = index_ / cache_assoc;
  c1.LRU_head = (Pcache_line *)malloc(c1.n_sets * sizeof(Pcache_line));

  for (int i = 0; i < c1.n_sets; i++) {
    c1.LRU_head[i] = (cache_line *)malloc(c1.associativity * sizeof(cache_line));
    for (int j = 0; j < c1.associativity; j++) {
      c1.LRU_head[i][j].tag = 0;
      c1.LRU_head[i][j].dirty = 0;

      //Linking
      if (i == 0 && j == 0) {
        c1.LRU_head[i][j].LRU_prev = NULL;
        c1.LRU_head[i][j].LRU_next = NULL;
      } else if (j == 0) {
        c1.LRU_head[i - 1][c1.associativity - 1].LRU_next = &c1.LRU_head[i][j];
        c1.LRU_head[i][j].LRU_prev = &c1.LRU_head[i - 1][c1.associativity - 1];
      } else {
        c1.LRU_head[i][j].LRU_prev = &c1.LRU_head[i][j - 1];
        c1.LRU_head[i][j - 1].LRU_next = &c1.LRU_head[i][j];
      }
    }
  }
  c1.LRU_tail = NULL;
}

/************************************************************/
cache_line *updateList(cache_line *head, cache_line *curr) {
  //for LRU policy
  if (curr == head) return head;

  // update the list: newest node as head, oldest node as tail.
  cache_line *prev = head;
  while (prev->LRU_next != curr) {
    prev = prev->LRU_next;
  }
  prev->LRU_next = curr->LRU_next;
  curr->LRU_next = head;
  return curr;
}

/************************************************************/
void perform_access(unsigned addr, unsigned access_type) {
  /* handle an access to the cache */
  bool hit = false; // default : false
  int offset_bits = log2(cache_block_size); //offset bits
  int index_bits = log2(c1.n_sets); //index bits
  int tag = addr >> (index_bits + offset_bits); //tag bit of address
  int index = (addr >> offset_bits) & (c1.n_sets - 1);

  int found_index = -1;
  for (int j = 0; j < c1.associativity; j++) {
    if (c1.LRU_head[index][j].tag == tag) { //hit
      hit = true;
      found_index = j;
      break;
    }
  }

  cache_stat *stat = (access_type == 2) ? &cache_stat_inst : &cache_stat_data;
  stat->accesses++;

  if (hit) {
    stat->demand_fetches++;
    if (cache_writeback && access_type != 2) {
      c1.LRU_head[index][found_index].dirty = 1;
    }
  } else {  //miss
    stat->misses++;
    stat->demand_fetches++;
    if (cache_writealloc || access_type == 2) {
      c1.LRU_head[index][0].tag = tag;
      c1.LRU_head[index][0].dirty = 0;
    }
  }
}

/************************************************************/
void flush() {
  /* flush the cache */
  for (int i = 0; i < c1.n_sets; i++) {
    for (int j = 0; j < c1.associativity; j++) {
      if (c1.LRU_head[i][j].dirty == 1) { //write memory
        c1.LRU_head[i][j].dirty = 0;
      }
    }
    free(c1.LRU_head[i]);
  }
  free(c1.LRU_head);
}

/************************************************************/
void delete(Pcache_line *head, Pcache_line *tail, Pcache_line item) {
  if (item->LRU_prev) {
    item->LRU_prev->LRU_next = item->LRU_next;
  } else {
    *head = item->LRU_next;  /* item at head */
  }

  if (item->LRU_next) {
    item->LRU_next->LRU_prev = item->LRU_prev;
  } else {
    *tail = item->LRU_prev; /* item at tail */
  }
}

/************************************************************/
/* inserts at the head of the list */
void insert(Pcache_line *head, Pcache_line *tail, Pcache_line item) {
  item->LRU_next = *head;
  item->LRU_prev = NULL;

  if (*head) {
    (*head)->LRU_prev = item;
  } else {
    *tail = item;
  }

  *head = item;
}

/************************************************************/
void dump_settings() {
  printf("*** CACHE SETTINGS ***\n");
  if (cache_split) {
    printf("  Split I- D-cache\n");
    printf("  I-cache size: \t%d\n", cache_isize);
    printf("  D-cache size: \t%d\n", cache_dsize);
  }
}
