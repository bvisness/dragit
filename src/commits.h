#pragma once

#include "orca_ext.h"
#include "util/strings.h"
#include <orca.h>

#define HASH_NUM_BUCKETS 32

u32 murmur3_str8(oc_str8 str);

typedef struct Commit {
  oc_str8 hash;
  oc_str8 authorName;
  oc_str8 summary;
  oc_str8_list parents; // parent hashes

  struct Commit *_hashNext;
} Commit;

typedef struct CommitTable {
  Commit *buckets[HASH_NUM_BUCKETS];
  i32 count;
} CommitTable;

typedef struct CommitListElem {
  oc_list_elt listElt;
  Commit *commit;
} CommitListElem;

void CommitTableInit(CommitTable *tab) {
  memset(&tab->buckets, 0, sizeof(tab->buckets));
}

void CommitTableInsert(CommitTable *tab, Commit *commit) {
  u32 key = murmur3_str8(commit->hash);
  Commit **ancestor = &tab->buckets[key % HASH_NUM_BUCKETS];
  Commit *chainCommit = *ancestor;
  while (chainCommit != NULL) {
    if (oc_str8_cmp(chainCommit->hash, commit->hash) == 0) {
      // git commits are content-addressed so just stop here, we found it.
      // no need to overwrite the contents; the contents are the same.
      return;
    }
    ancestor = &chainCommit->_hashNext;
    chainCommit = *ancestor;
  }
  *ancestor = commit;
  tab->count += 1;
}

Commit *CommitTableGet(CommitTable *tab, oc_str8 hash) {
  u32 key = murmur3_str8(hash);
  Commit *chainCommit = tab->buckets[key % HASH_NUM_BUCKETS];
  while (chainCommit != NULL) {
    if (oc_str8_cmp(chainCommit->hash, hash) == 0) {
      return chainCommit;
    }
    chainCommit = chainCommit->_hashNext;
  }
  return NULL;
}

static inline u32 rotl32(u32 x, i8 r) { return (x << r) | (x >> (32 - r)); }

static inline u32 fmix(u32 h) {
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  return h ^= h >> 16;
}

// courtesy of demetri spanos
// https://discord.com/channels/239737791225790464/241302871927291915/710202956980420738
u32 murmur3(const void *key, int len, u32 h1) {
  const u8 *tail = (const u8 *)(key + (len / 4) * 4); // handle this separately

  u32 c1 = 0xcc9e2d51, c2 = 0x1b873593;

  // body (full 32-bit blocks) handled uniformly
  for (u32 *p = (u32 *)key; p < (const u32 *)tail; p++) {
    u32 k1 = *p;
    k1 *= c1;
    k1 = rotl32(k1, 15);
    k1 *= c2; // MUR1
    h1 ^= k1;
    h1 = rotl32(h1, 13);
    h1 = h1 * 5 + 0xe6546b64; // MUR2
  }

  u32 t = 0; // handle up to 3 tail bytes
  switch (len & 3) {
  case 3:
    t ^= tail[2] << 16;
  case 2:
    t ^= tail[1] << 8;
  case 1: {
    t ^= tail[0];
    t *= c1;
    t = rotl32(t, 15);
    t *= c2;
    h1 ^= t;
  };
  }
  return fmix(h1 ^ len);
}

u32 murmur3_str8(oc_str8 str) { return murmur3(str.ptr, str.len, 0); }

void log_commit(Commit *commit) {
  oc_log_info("commit %.*s\n  %.*s\n  %.*s\n", oc_str8_printf(commit->hash),
              oc_str8_printf(commit->authorName),
              oc_str8_printf(commit->summary));
}
