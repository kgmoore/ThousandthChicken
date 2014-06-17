// License: please see LICENSE2 file for more details.

#pragma once
#include "DeviceKernel.h"
#include <list>
#include "codestream_image.h"

#define MAX_CODESTREAM_SIZE (4096 * 4) /// TODO: figure out

#define LL_LH_SUBBAND	0
#define HL_SUBBAND		1
#define HH_SUBBAND		2

typedef struct _CodeBlockAdditionalInfo CodeBlockAdditionalInfo;


typedef struct
{
	int significantBits;
	int codingPasses;
	int subband;
	int width;
	int height;

	int nominalWidth;
	int nominalHeight;

	int magbits;
	int compType;
	int dwtLevel;
	float stepSize;

	unsigned char *codestream;
	int length;
	type_codeblock *cblk;
} EntropyCodingTaskInfo;

class CoefficientCoder : 	public DeviceKernel
{
public:
	CoefficientCoder(KernelInitInfoBase initInfo);
	virtual ~CoefficientCoder(void);
	void decode_tile(type_tile *tile);
private:
	void decodeInit(EntropyCodingTaskInfo *infos, int count, void** coefficients);
	float decode(int codeBlocks);
	void convert_to_decoding_task(EntropyCodingTaskInfo &task, type_codeblock &cblk, int& offset);
	void extract_cblks(type_tile *tile, std::list<type_codeblock *> &out_cblks);

	unsigned char* h_codestreamBuffers;
	CodeBlockAdditionalInfo *h_infos;
	cl_mem d_decodedCoefficientsBuffers ;
	cl_mem d_codestreamBuffers;
	cl_mem d_stBuffers;
	cl_mem d_infos;

};

