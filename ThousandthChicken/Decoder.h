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
private:

	ocl_args_d_t* _ocl;
	CoefficientCoder coder;
	Quantizer quantizer;
	DWT dwt;
	Preprocessor preprocessor;


};

