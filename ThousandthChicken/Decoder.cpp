#include "platform.h"

#include "ocl_util.h"

#include "Decoder.h"

#include <stdio.h>
#include <stdlib.h>
#include "codestream_image_types.h"
#include "config_parameters.h"
#include "logger.h"
#include "boxes.h"
#include "io_buffered_stream.h"
#include "codestream.h"
#include "basic.h"




Decoder::Decoder(ocl_args_d_t* ocl) : _ocl(ocl),
									coder( KernelInitInfoBase(_ocl->commandQueue, /*"-g -s \"c:\\src\\ThousandthChicken\\ThousandthChicken\\coefficient_coder.cl\""*/"")),
									quantizer( KernelInitInfoBase(_ocl->commandQueue, /*"-g -s \"c:\\src\\ThousandthChicken\\ThousandthChicken\\quantizer.cl\""*/"")),
									dwt(KernelInitInfoBase(_ocl->commandQueue, "")),
									preprocessor(KernelInitInfoBase(_ocl->commandQueue, ""))


	                     
{
}


Decoder::~Decoder(void)
{
}

void init_dec_buffer(FILE *fsrc, type_buffer *src_buff) {
	fseek(fsrc, 0, SEEK_END);
	long file_length = ftell(fsrc);
	fseek(fsrc, 0, SEEK_SET);

	src_buff->data = (unsigned char *) malloc(file_length);
	src_buff->size = file_length;

	fread(src_buff->data, 1, file_length, fsrc);

	src_buff->start = src_buff->data;
	src_buff->end = src_buff->data + src_buff->size;
	src_buff->bp = src_buff->data;
	src_buff->bits_count = 0;
	src_buff->byte = 0;
}


int Decoder::decode(std::string fileName)
{
	//	println_start(INFO);
	type_image *img = (type_image *)malloc(sizeof(type_image));
	memset(img, 0, sizeof(type_image));
	img->in_file = fileName.c_str();
	type_parameters *param = (type_parameters*)malloc(sizeof(type_parameters));
	default_config_values(param);
	//init_device(param);

	FILE *fsrc = fopen(img->in_file, "rb");
	if (!fsrc) {
		fprintf(stderr, "Error, failed to open %s for reading\n", img->in_file);
		return 1;
	}

	type_tile *tile;
	unsigned int i,j;

	if(strstr(img->in_file, ".jp2") != NULL) {
		println(INFO, "It's a JP2 file");

		//parse the JP2 boxes
		jp2_parse_boxes(fsrc, img);
		fclose(fsrc);

		for (i = 0; i < img->num_tiles; i++) {
			type_tile* tile = img->tile + i;

			for (j = 0; j < tile->parent_img->num_components; j++) {
				//		println_var(INFO, "%d\n", i);
				type_tile_comp* tile_comp = tile->tile_comp + j;

				
				//allocate image tile component memory on device 
				cl_int err = CL_SUCCESS;
				int dataTypeSize = tile->parent_img->wavelet_type ? sizeof(float) : sizeof(int);
				tile_comp->img_data_d = (void*)clCreateBuffer(_ocl->context, CL_MEM_READ_WRITE, tile_comp->width * tile_comp->height * dataTypeSize, NULL, &err);
				SAMPLE_CHECK_ERRORS(err);
				if (tile_comp->img_data_d  == 0)
					throw Error("Failed to create tile component Buffer!");

			}
		}

		// Do decoding for all tiles
		for(i = 0; i < img->num_tiles; i++) {
			tile = &(img->tile[i]);
			/* Decode data */
			coder.decode_tile(tile);
			/* Dequantize data */
			quantizer.dequantize_tile(tile);
			/* Do inverse wavelet transform */
			dwt.iwt(tile);
		}

		if(img->use_mct == 1) {
			// lossless decoder
			if(img->wavelet_type == 0) {
				preprocessor.color_decoder_lossless(img);
			}
			else { //lossy decoder
				preprocessor.color_decoder_lossy(img);
			}
		} else if (img->use_part2_mct == 1) {
			//klt.decode_klt(img);
			//part 2 not supported
		} else {
			if(img->sign == UNSIGNED) {
				preprocessor.idc_level_shifting(img);
			}
		}
	}
	else {//It is not a JP2 file.
		type_buffer *src_buff = (type_buffer *) malloc(sizeof(type_buffer));

		init_dec_buffer(fsrc, src_buff);
		fclose(fsrc);

		decode_codestream(src_buff, img);

	//	get_next_box(fsrc);

		// Do decoding for all tiles
		for(i = 0; i < img->num_tiles; i++)	{
			tile = &(img->tile[i]);
			/* Decode data */
			coder.decode_tile(tile);
			/* Dequantize data */
			quantizer.dequantize_tile(tile);
			/* Do inverse wavelet transform */
			dwt.iwt(tile);
		}

		if(img->use_mct == 1) {
			// lossless decoder
			if(img->wavelet_type == 0) {
				preprocessor.color_decoder_lossless(img);
			}
			else {  //lossy decoder
				preprocessor.color_decoder_lossy(img);
			}
		} else if (img->use_part2_mct == 1) {
			//klt.decode_klt(img);
			//part 2 not supported
		} else {
			if(img->sign == UNSIGNED) {
				preprocessor.idc_level_shifting(img);
			}
		}
	}
	return 0;
}