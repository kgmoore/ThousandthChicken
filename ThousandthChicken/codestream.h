// License: please see LICENSE2 file for more details.

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "codestream_image_types.h"
#include "io_buffered_stream.h"

/** Packet parameters */
typedef struct _type_packet{
	unsigned short *inclusion;
	unsigned short *zero_bit_plane;
	unsigned short *num_coding_passes;
} type_packet;

void decode_codestream(type_buffer *buffer, type_image *img);



#ifdef __cplusplus
}
#endif