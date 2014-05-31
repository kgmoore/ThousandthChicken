#pragma once

struct ocl_args_d_t;

class Decoder
{
public:
	Decoder(void);
	~Decoder(void);

	int decode(ocl_args_d_t* ocl);
};

