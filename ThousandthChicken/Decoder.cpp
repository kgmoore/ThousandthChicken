// License: please see LICENSE1 file for more details.

#include "platform.h"

#include "ocl_util.h"

#include "Decoder.h"

#include <stdio.h>
#include <stdlib.h>
#include "codestream_image_types.h"
#include "logger.h"
#include "boxes.h"
#include "io_buffered_stream.h"
#include "codestream.h"
#include "basic.h"
#include <time.h>
#include "MemoryMapped.h"


Decoder* decoder = NULL;

void handleCodeBlock(type_codeblock* cblk, unsigned char* codestream) {
	if (decoder)
		decoder->parsedCodeBlock(cblk, codestream);

}




Decoder::Decoder(ocl_args_d_t* ocl) : _ocl(ocl),
	                                  coder(NULL),
									  quantizer(NULL),
									  dwt(NULL),
									  preprocessor(NULL),
									  dev_alignment(128)
{
	coder = new  CoefficientCoder(KernelInitInfoBase(_ocl->commandQueue, /*"-g -s \"c:\\src\\ThousandthChicken\\ThousandthChicken\\coefficient_coder.cl\""*/""));
	quantizer = new Quantizer(KernelInitInfoBase(_ocl->commandQueue, /*"-g -s \"c:\\src\\ThousandthChicken\\ThousandthChicken\\quantizer.cl\""*/""));
	dwt = new DWT(KernelInitInfoBase(_ocl->commandQueue, ""));
	preprocessor = new Preprocessor(KernelInitInfoBase(_ocl->commandQueue, ""));
	dev_alignment = requiredOpenCLAlignment(_ocl->device);
	codeBlockCallback = handleCodeBlock;
}


Decoder::~Decoder(void)
{
	if (coder)
		delete coder;
	if (quantizer)
		delete quantizer;
	if (dwt)
		delete dwt;
	if (preprocessor)
		delete preprocessor;
}

void init_dec_buffer(unsigned char* data, unsigned long int dataLength, type_buffer *src_buff) {
	src_buff->data = data;
	src_buff->size = dataLength;

	src_buff->start = src_buff->data;
	src_buff->end = src_buff->data + src_buff->size;
	src_buff->bp = src_buff->data;
	src_buff->bits_count = 0;
	src_buff->byte = 0;
}

void Decoder::parsedCodeBlock(type_codeblock* cblk, unsigned char* codestream) {

	cblk->codestream = (unsigned char*)aligned_malloc(cblk->length, dev_alignment);
	memcpy(cblk->codestream, codestream, cblk->length);

	//1. enqueue write to device memory

	//2. when enough codeblocks have been parsed, launch kernel
}

cl_int Decoder::mapComponentToHost(type_tile_comp* tile_comp){
		
	cl_int error_code = CL_SUCCESS;
	tile_comp->img_data_h = clEnqueueMapBuffer(_ocl->commandQueue, (cl_mem)tile_comp->img_data_d, true, CL_MAP_READ, 
		                                0, tile_comp->width * tile_comp->height * sizeof(int), 0, NULL, NULL, &error_code);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: clEnqueueMapBuffer return %s.\n", TranslateOpenCLError(error_code));
    }
	return error_code;
}


int Decoder::decode(std::string fileName)
{
	decoder = this;
	type_image *img = (type_image *)malloc(sizeof(type_image));
	memset(img, 0, sizeof(type_image));
	img->in_file = fileName.c_str();
	
	// map file to memory
	MemoryMapped data(fileName, MemoryMapped::WholeFile, MemoryMapped::SequentialScan);
	if (!data.isValid())
	{
	printf("File not found\n");
	return -2;
	}

  // raw pointer to mapped memory
    unsigned char* buffer = (unsigned char*)data.getData();

	type_buffer *src_buff = (type_buffer *) malloc(sizeof(type_buffer));
	memset(src_buff, 0, sizeof(type_buffer));
	src_buff->data = buffer;
	src_buff->bp = buffer;
	src_buff->size = data.size();

	double t1 = time_stamp();
	type_tile *tile;
	unsigned int i,j;
	if(strstr(img->in_file, ".jp2") != NULL) {
		println(INFO, "It's a JP2 file");

		//parse the JP2 boxes
		jp2_parse_boxes(src_buff, img);

		for (i = 0; i < img->num_tiles; i++) {
			type_tile* tile = img->tile + i;

			for (j = 0; j < tile->parent_img->num_components; j++) {
				type_tile_comp* tile_comp = tile->tile_comp + j;
				
				//allocate image tile component memory on device 
				cl_int err = CL_SUCCESS;
				tile_comp->img_data_h = NULL;
				tile_comp->img_data_d = (void*)clCreateBuffer(_ocl->context, CL_MEM_READ_WRITE, tile_comp->width * tile_comp->height * sizeof(int), NULL, &err);
				SAMPLE_CHECK_ERRORS(err);
				if (tile_comp->img_data_d  == 0)
					throw Error("Failed to create tile component Buffer!");

			}
		}
	} else {
		
		type_buffer *src_buff = (type_buffer *) malloc(sizeof(type_buffer));
		init_dec_buffer(buffer, data.size(), src_buff);
		decode_codestream(src_buff, img);
		free(src_buff);
		
	}

	// Do decoding for all tiles
	for(i = 0; i < img->num_tiles; i++) {
		tile = img->tile + i;
		if (coder)
			coder->decode_tile(tile);
		if (quantizer)
		     quantizer->dequantize_tile(tile);
		if (dwt)
			dwt->iwt(tile);
	}

	if (preprocessor) {
		if(img->use_mct == 1) {
			// lossless decoder
			if(img->wavelet_type == 0) {
				preprocessor->color_decoder_lossless(img);
			}
			else { //lossy decoder
				preprocessor->color_decoder_lossy(img);
			}
		} else if (img->use_part2_mct == 1) {
			//klt.decode_klt(img);
			//part 2 not supported
		} else {
			if(img->sign == UNSIGNED) {
				preprocessor->idc_level_shifting(img);
			}
		}
	}

	clFinish(_ocl->commandQueue);
	//map component memory from device to host
	for (i = 0; i < img->num_tiles; i++) {
		type_tile* tile = img->tile + i;
		for (j = 0; j < tile->parent_img->num_components; j++) {
			    mapComponentToHost( tile->tile_comp + j);
		}
	}

	double t2 = time_stamp();
	int diff =  (int)((t2 - t1)*1000);
	printf("Decode time: %d ms ",diff);

	//release tile component device memory
	cl_int error_code = CL_SUCCESS;
	for (i = 0; i < img->num_tiles; i++) {
		type_tile* tile = img->tile + i;
		for (j = 0; j < tile->parent_img->num_components; j++) {
			type_tile_comp* comp = tile->tile_comp + j;
			error_code = clEnqueueUnmapMemObject(_ocl->commandQueue,(cl_mem)comp->img_data_d, comp->img_data_h,0,NULL,NULL);
			if (CL_SUCCESS != error_code)
			{
				LogError("Error: clEnqueueUnmapMemObject return %s.\n", TranslateOpenCLError(error_code));
			}
			error_code = clReleaseMemObject((cl_mem)comp->img_data_d);
			if (CL_SUCCESS != error_code)
			{
				LogError("Error: clReleaseMemObject return %s.\n", TranslateOpenCLError(error_code));
			}
		}
	}

	free_image(img);
	scanf("%d");
	return 0;
}