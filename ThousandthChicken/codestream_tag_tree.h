#pragma once

typedef struct type_tag_tree_node type_tag_tree_node;
typedef struct type_tag_tree type_tag_tree;

/**
 * Tag node
 */
struct type_tag_tree_node {
  type_tag_tree_node *parent;
  int value;
  int low;
  int known;
};

/**
 * Tag tree
 */
struct type_tag_tree {
  int num_leafs_h;
  int num_leafs_v;
  int num_nodes;
  type_tag_tree_node *nodes;
};

