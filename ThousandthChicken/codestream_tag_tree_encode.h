// License: please see LICENSE2 file for more details.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "codestream_tag_tree.h"
#include "codestream_image_types.h"
#include "io_buffered_stream.h"

type_tag_tree *tag_tree_create(int num_leafs_h, int num_leafs_v);
int decode_tag_tree(type_buffer *buffer, type_tag_tree *tree, int leaf_no, int threshold);
void tag_tree_reset(type_tag_tree *tree);

#ifdef __cplusplus
}
#endif