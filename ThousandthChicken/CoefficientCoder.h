#pragma once
#include "GenericKernel.h"
#include <list>
#include "codestream_image.h"

#define MAX_CODESTREAM_SIZE (4096 * 2) /// TODO: figure out

#define LL_LH_SUBBAND	0
#define HL_SUBBAND		1
#define HH_SUBBAND		2


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

	unsigned char *codeStream;
	int length;
	type_codeblock *cblk;
} EntropyCodingTaskInfo;

class CoefficientCoder : 	public GenericKernel
{
public:
	CoefficientCoder(KernelInitInfoBase initInfo);
	virtual ~CoefficientCoder(void);
	void decode_tile(type_tile *tile);
private:
	float gpuDecode(EntropyCodingTaskInfo *infos, int count, void** coefficients);
	void convert_to_decoding_task(EntropyCodingTaskInfo &task, type_codeblock &cblk);
	void extract_cblks(type_tile *tile, std::list<type_codeblock *> &out_cblks);

};

