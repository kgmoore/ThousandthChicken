// License: please see LICENSE2 file for more details.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

	
#include "codestream_image_types.h"
#include "config_parameters.h"

void init_tiles(type_image *img, type_parameters *param);
void free_image(type_image* img);


#ifdef __cplusplus
}
#endif