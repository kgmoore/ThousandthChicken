#include "codestream_tag_tree_encode.h"
#include "logger.h"
#include <stdlib.h>


void tag_tree_reset(type_tag_tree *tree)
{
	int i;
	if (!tree) {
		println_var(INFO, "Error: trying to reset null tag tree");
		return;
	}
	for (i = 0; i < tree->num_nodes; i++) {
		tree->nodes[i].value = 999;
		tree->nodes[i].low = 0;
		tree->nodes[i].known = 0;
	}
}

type_tag_tree *tag_tree_create(int num_leafs_h, int num_leafs_v)
{
	int nplh[32];
	int nplv[32];
	type_tag_tree_node *node = NULL;
	type_tag_tree_node *parent_node = NULL;
	type_tag_tree_node *parent_node0 = NULL;
	type_tag_tree *tree = NULL;
	int i, j, k;
	int num_lvls;
	int n;

	tree = (type_tag_tree *) malloc(sizeof(type_tag_tree));
	if (!tree) {
		println_var(INFO, "Error: unable to allocate tag tree");
		return NULL;
	}
	tree->num_leafs_h = num_leafs_h;
	tree->num_leafs_v = num_leafs_v;

	num_lvls = 0;
	nplh[0] = num_leafs_h;
	nplv[0] = num_leafs_v;
	tree->num_nodes = 0;
	do {
		n = nplh[num_lvls] * nplv[num_lvls];
		nplh[num_lvls + 1] = (nplh[num_lvls] + 1) / 2;
		nplv[num_lvls + 1] = (nplv[num_lvls] + 1) / 2;
		tree->num_nodes += n;
		++num_lvls;
	} while (n > 1);

	if (tree->num_nodes == 0) {
		free(tree);
		println_var(INFO, "Error: tag tree with zero nodes");
		return NULL;
	}
	tree->nodes = (type_tag_tree_node*) calloc(tree->num_nodes, sizeof(type_tag_tree_node));
	if (!tree->nodes) {
		free(tree);
		println_var(INFO, "Error: tag tree with null nodes");
		return NULL;
	}

	node = tree->nodes;
	parent_node = &tree->nodes[tree->num_leafs_h * tree->num_leafs_v];
	parent_node0 = parent_node;

	for (i = 0; i < num_lvls - 1; ++i) {
		for (j = 0; j < nplv[i]; ++j) {
			k = nplh[i];
			while (--k >= 0) {
				node->parent = parent_node;
				++node;
				if (--k >= 0) {
					node->parent = parent_node;
					++node;
				}
				++parent_node;
			}
			if ((j & 1) || j == nplv[i] - 1) {
				parent_node0 = parent_node;
			} else {
				parent_node = parent_node0;
				parent_node0 += nplh[i];
			}
		}
	}
	node->parent = 0;
	tag_tree_reset(tree);
	return tree;
}

int decode_tag_tree(type_buffer *buffer, type_tag_tree *tree, int leaf_no, int threshold)
{
	type_tag_tree_node *stk[31];
	type_tag_tree_node **stkptr;
	type_tag_tree_node *node;
	int low;

	stkptr = stk;
	node = &tree->nodes[leaf_no];
	while (node->parent) {
		*stkptr++ = node;
		node = node->parent;
	}

	low = 0;
	for (;;) {
		if (low > node->low) {
			node->low = low;
		} else {
			low = node->low;
		}
		while (low < threshold && low < node->value) {
			if (read_bits(buffer, 1)) {
				node->value = low;
			} else {
				++low;
			}
		}
		node->low = low;
		if (stkptr == stk) {
			break;
		}
		node = *--stkptr;
	}
	return (node->value < threshold) ? 1 : 0;
}
