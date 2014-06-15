// License: please see LICENSE1 file for more details.

#pragma once

#include "CoefficientCoder.h"
#include "Quantizer.h"
#include "DWT.h"
#include "Preprocessor.h"
#include <string>


struct ocl_args_d_t;

class Decoder
{
public:
	Decoder(ocl_args_d_t* ocl);
	~Decoder(void);
	int decode(std::string fileName);
	void parsedCodeBlock(type_codeblock* cblk, unsigned char* codestream);
private:

	ocl_args_d_t* _ocl;
	CoefficientCoder* coder;
	Quantizer* quantizer;
	DWT* dwt;
	Preprocessor* preprocessor;

	cl_uint dev_alignment ;
	cl_int mapComponentToHost(type_tile_comp* tile_comp);


};

