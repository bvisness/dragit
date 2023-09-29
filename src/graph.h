#pragma once

#include "commits.h"
#include "util/memory.h"

// Easily enough for a demo without ever needing to grow.
#define SCRATCH_NODES 100

typedef struct Node {
  Commit *commit;
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
