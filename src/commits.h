#pragma once

#include "orca_ext.h"
#include "util/debug.h"
#include "util/memory.h"
#include "util/strings.h"
#include <orca.h>

#define HASH_NUM_BUCKETS 512

u32 murmur3_str8(oc_str8 str);

typedef struct Commit {
  oc_str8 hash;
  oc_str8 authorName;
  oc_str8 summary;
  oc_str8_list parents;
  oc_str8_list children;

  struct Node *node;

  struct Commit *_hashNext;
} Commit;

typedef struct CommitTable {
  oc_arena *arena;
  Commit *buckets[HASH_NUM_BUCKETS];
  i32 count;
} CommitTable;

typedef struct CommitListElem {
  oc_list_elt listElt;
  Commit *commit;
} CommitListElem;

void CommitTableInit(oc_arena *arena, CommitTable *tab) {
  memset(&tab->buckets, 0, sizeof(tab->buckets));
  tab->arena = arena;
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
  oc_log_info("commit %.*s\n  %.*s\n  %.*s", oc_str8_printf(commit->hash),
              oc_str8_printf(commit->authorName),
              oc_str8_printf(commit->summary));
}

// Easily enough for a demo without ever needing to grow.
#define SCRATCH_NODES 100

typedef struct Node {
  Commit *commit;

  // bigger = farther from root = higher up.
  // 1 is the minimum!! 0 means unknown.
  i32 depth;
  i32 track; // bigger = further right
  i32 trackOffsetFromParent;

  bool boring;
  bool omitted;

  // Computed layout values
  f32 x, y;
  f32 targetX, targetY;
} Node;

typedef struct NodeList {
  Node *nodes;
  i32 count;
  i32 cap;
} NodeList;

void NodeListInit(oc_arena *arena, NodeList *list, i32 size) {
  i32 cap = size + SCRATCH_NODES;
  *list = (NodeList){
      .nodes = oc_arena_push_array(arena, Node, cap),
      .count = 0,
      .cap = cap,
  };
}

Node *NodeListPush(NodeList *list, Node node) {
  i32 i = list->count;
  list->nodes[i] = node;
  list->count += 1;
  return &list->nodes[i];
};

void NodeListDelete(NodeList *list, i32 i) { list->nodes[i] = (Node){0}; }

#define NODE_QUEUE_CAP 128

typedef struct NodeQueue {
  Node *nodes[NODE_QUEUE_CAP];
  i32 count;
} NodeQueue;

void NodeQueueInit(NodeQueue *queue) { *queue = (NodeQueue){}; }

void NodeQueuePush(NodeQueue *queue, Node *node) {
  // Check for duplicate
  for (i32 i = 0; i < queue->count; i++) {
    if (node == queue->nodes[i]) {
      return;
    }
  }

  OC_ASSERT(queue->count < NODE_QUEUE_CAP, "node queue is full");
  queue->nodes[queue->count] = node;
  queue->count += 1;
}

Node *NodeQueueRemove(NodeQueue *queue, i32 i) {
  OC_ASSERT(0 <= i && i < queue->count, "cannot remove from empty node queue");
  Node *node = queue->nodes[i];
  for (int j = i; j < queue->count - 1; j++) {
    queue->nodes[j] = queue->nodes[j + 1];
  }
  queue->count -= 1;
  queue->nodes[queue->count] = NULL;
  return node;
}

// Desired outcome:
// - Root node is depth 1
// - Recurse to all node children.
// - If a commit has one child and one parent, it is boring.
//    - Except, if the commit participated in a merge, it is not boring.
// - Boring commits will be omitted by default on startup.
// - Runs of omitted commits have the same depth.
// - Every commit has depth > all its parents.
void computeNodeDepths(CommitTable *commits, Node *node, Node *parent,
                       bool setOmittedIfBoring) {
  bool isMerged = false;
  oc_str8_list_for(node->commit->children, childHash) {
    Commit *child = CommitTableGet(commits, childHash->string);
    if (child->parents.eltCount > 1) {
      // Child is merge commit, therefore this commit was merged into it!
      isMerged = true;
    }
  }
  bool isBoring = node->commit->parents.eltCount == 1 &&
                  node->commit->children.eltCount == 1 && !isMerged;

  node->boring = isBoring;
  if (isBoring && setOmittedIfBoring) {
    node->omitted = true;
  }

  i32 newDepth = 1;
  if (parent) {
    newDepth =
        (parent->omitted && node->omitted) ? parent->depth : parent->depth + 1;
  }
  node->depth = oc_max_i32(node->depth, newDepth);

  // TODO: Doing this depth-first is really runaway
  // expensive in the face of lots of merging and joining. A
  // breadth-first traversal would be dramatically cheaper.

  oc_str8_list_for(node->commit->children, childHash) {
    Commit *child = CommitTableGet(commits, childHash->string);
    computeNodeDepths(commits, child->node, node, setOmittedIfBoring);
  }
}

// Starting at the root node, work up the tree and adjust all child nodes so
// that nodes do not overlap.
void fixupTracks(oc_arena *arena, CommitTable *commits, Node *root) {
  NodeQueue *queue = oc_arena_push_type(arena, NodeQueue);
  NodeQueueInit(queue);
  NodeQueuePush(queue, root);

  int currentDepth = root->depth;
  while (true) {
    if (queue->count == 0) {
      break;
    }

    // Adjust the tracks of all runs of nodes at the same depth. (Such runs are
    // of omitted commits, which share the same place in the graph.)
    for (i32 i = 0; i < queue->count; i++) {
      Node *node = queue->nodes[i];
      OC_ASSERT(node->depth >= currentDepth);

      // First move each node to the minimum of its parents' tracks
      i32 minParentTrack = node == root ? 0 : 999;
      oc_str8_list_for(node->commit->parents, parentHash) {
        Node *parent = CommitTableGet(commits, parentHash->string)->node;
        minParentTrack = oc_min_i32(minParentTrack, parent->track);
      }
      node->track = minParentTrack + node->trackOffsetFromParent;

      i32 trackAdjust = 0;
      if (node->depth == currentDepth) {
        // Iterate through commit's children and adjust each "set" of commits
        // horizontally. A "set" is a run of boring commits followed by an
        // interesting commit. (Typically there are no boring commits.)
        oc_str8_list_for(node->commit->children, childHash) {
          Node *child = CommitTableGet(commits, childHash->string)->node;
          child->trackOffsetFromParent = trackAdjust;
          trackAdjust += 1;
          NodeQueuePush(queue, child);
        }

        NodeQueueRemove(queue, i);
        i -= 1; // counteract the loop increment
      }
    }

    currentDepth += 1;
  }
}

void layoutNodes(oc_arena *arena, NodeList *nodes, CommitTable *commits,
                 Node *root, bool setOmittedIfBoring) {
  for (int i = 0; i < nodes->count; i++) {
    Node *node = &nodes->nodes[i];
    node->depth = 0;
    node->track = 0;
  }
  oc_log_info("computing node depths");
  computeNodeDepths(commits, root, NULL, setOmittedIfBoring);
  oc_log_info("adjusting node tracks");
  fixupTracks(arena, commits, root);
}

void nodeSetOmitted(Node* node, bool omitted) {
  if (node->boring) {
    node->omitted = omitted;
  } else {
    // Interesting nodes cannot be omitted
    node->omitted = false;
  }
}
