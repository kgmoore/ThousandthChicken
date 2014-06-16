// License: please see LICENSE2 file for more details.

#include "CoefficientCoder.h"

#include "coefficientcoder_common.h"
#include "codestream_image_types.h"

#include "basic.h"

#ifdef __linux__
#include <sys/time.h>
#include <unistd.h>
#include <libgen.h>
#elif defined(_WIN32) || defined(WIN32)
#include <windows.h>
#else
#include <ctime>
#endif

using namespace std;


CoefficientCoder::CoefficientCoder(KernelInitInfoBase initInfo) : 
						DeviceKernel( KernelInitInfo(initInfo, "coefficient_coder.cl", "g_decode") ),
						h_codestreamBuffers(NULL),
	                    h_infos(NULL),
						 d_decodedCoefficientsBuffers(0),
						 d_codestreamBuffers(0),
						 d_stBuffers(0),
						 d_infos(0)

{
}


CoefficientCoder::~CoefficientCoder(void)
{
	if (d_infos) {
		cl_int err = clReleaseMemObject(d_infos);
		SAMPLE_CHECK_ERRORS(err);
	}

	if (d_stBuffers) {
		cl_int err = clReleaseMemObject(d_stBuffers);
		SAMPLE_CHECK_ERRORS(err);

	}

	if (d_codestreamBuffers) {
		cl_int err = clReleaseMemObject(d_codestreamBuffers);
		SAMPLE_CHECK_ERRORS(err);

	}

	if (h_infos)
	   aligned_free(h_infos);
	if (h_codestreamBuffers)
	   aligned_free(h_codestreamBuffers);
}


void CoefficientCoder::decode_tile(type_tile *tile)
{
//	println_start(INFO);

	std::list<type_codeblock *> cblks;
	extract_cblks(tile, cblks);

	EntropyCodingTaskInfo *tasks = (EntropyCodingTaskInfo *) calloc( cblks.size(),sizeof(EntropyCodingTaskInfo));

	std::list<type_codeblock *>::iterator ii = cblks.begin();

	int num_tasks = 0;
	int offset = 0;
	for(; ii != cblks.end(); ++ii)
	{
		convert_to_decoding_task(tasks[num_tasks++], *(*ii),offset);
	}

//	printf("%d\n", num_tasks);
	decodeInit(tasks, num_tasks, &tile->coefficients);
	float t = decode(tasks, num_tasks, &tile->coefficients);

	printf("coefficient decoder kernel consumption: %f ms\n", t);
	free(tasks);

//	println_end(INFO);
}

void CoefficientCoder::extract_cblks(type_tile *tile, std::list<type_codeblock *> &out_cblks)
{
	for(int i = 0; i < tile->parent_img->num_components; i++)
	{
		type_tile_comp *tile_comp = &(tile->tile_comp[i]);
		for(int j = 0; j < tile_comp->num_rlvls; j++)
		{
			type_res_lvl *res_lvl = &(tile_comp->res_lvls[j]);
			for(int k = 0; k < res_lvl->num_subbands; k++)
			{
				type_subband *sb = &(res_lvl->subbands[k]);
				for(unsigned int l = 0; l < sb->num_cblks; l++)
					out_cblks.push_back(&(sb->cblks[l]));
			}
		}
	}
}
void CoefficientCoder::convert_to_decoding_task(EntropyCodingTaskInfo &task, type_codeblock &cblk, int& offset)
{
	switch(cblk.parent_sb->orient)
	{
	case LL:
	case LH:
		task.subband = 0;
		break;
	case HL:
		task.subband = 1;
		break;
	case HH:
		task.subband = 2;
		break;
	}

	task.width = cblk.width;
	task.height = cblk.height;

	task.nominalWidth = cblk.parent_sb->parent_res_lvl->parent_tile_comp->cblk_w;
	task.nominalHeight = cblk.parent_sb->parent_res_lvl->parent_tile_comp->cblk_h;

	cblk.d_coefficientsOffset = offset;
	offset += task.nominalWidth * task.nominalHeight;

	task.magbits = cblk.parent_sb->mag_bits;

	task.codestream = cblk.codestream;
	task.length = cblk.length;
	task.significantBits = cblk.significant_bits;
}


void CoefficientCoder::decodeInit(EntropyCodingTaskInfo *infos, int count, void** coefficients)
{
    cl_int err = CL_SUCCESS;

	int codeBlocks = count;
	int maxOutLength = MAX_CODESTREAM_SIZE;

    // execute kernel
	cl_uint dev_alignment = requiredOpenCLAlignment(device);

	// allocate codestream buffer on host
	h_codestreamBuffers = (unsigned char*)aligned_malloc(codeBlocks * maxOutLength,dev_alignment);
    if (h_codestreamBuffers == NULL)
        throw Error("Failed to create h_codestreamBuffer Buffer!");


	// allocate h_infos on host
	h_infos = (CodeBlockAdditionalInfo *) aligned_malloc(sizeof(CodeBlockAdditionalInfo) * codeBlocks,dev_alignment);
	   if (h_infos == NULL)
        throw Error("Failed to create h_infos Buffer!");

    //initialize h_infos
	int magconOffset = 0;
	int coefficientsOffset = 0;
	for(int i = 0; i < codeBlocks; i++)
	{
		h_infos[i].width = infos[i].width;
		h_infos[i].height = infos[i].height;
		h_infos[i].nominalWidth = infos[i].nominalWidth;
		h_infos[i].nominalHeight = infos[i].nominalHeight;
		h_infos[i].stripeNo = ceil(infos[i].height / 4.0f);
		h_infos[i].subband = infos[i].subband;
		h_infos[i].magconOffset = magconOffset + infos[i].width;
		h_infos[i].magbits = infos[i].magbits;
		h_infos[i].length = infos[i].length;
		h_infos[i].significantBits = infos[i].significantBits;
		h_infos[i].d_coefficientsOffset = coefficientsOffset;
		coefficientsOffset +=  infos[i].nominalWidth * infos[i].nominalHeight;

	    //copy each code block codestream buffer to host memory block
		memcpy(h_codestreamBuffers + i * maxOutLength, infos[i].codestream, infos[i].length);
		magconOffset += h_infos[i].width * (h_infos[i].stripeNo + 2);
	}

	//allocate d_coefficients on device
	d_decodedCoefficientsBuffers = clCreateBuffer(context, CL_MEM_READ_WRITE ,  sizeof(int) * coefficientsOffset, NULL, &err);
    SAMPLE_CHECK_ERRORS(err);
    if (d_decodedCoefficientsBuffers == (cl_mem)0)
        throw Error("Failed to create d_decodedCoefficientsBuffers Buffer!");

	//allocate d_codestreamBuffer on device and pin to host memory
	d_codestreamBuffers = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, codeBlocks * maxOutLength, h_codestreamBuffers, &err);
    SAMPLE_CHECK_ERRORS(err);
    if (d_codestreamBuffers == (cl_mem)0)
        throw Error("Failed to create d_codestreamBuffers Buffer!");

	//allocate d_stBuffers on device and initialize it to zero
	d_stBuffers = clCreateBuffer(context, CL_MEM_READ_WRITE ,  sizeof(unsigned int) * magconOffset, NULL, &err);
    SAMPLE_CHECK_ERRORS(err);
    if (d_stBuffers == (cl_mem)0)
        throw Error("Failed to create d_infos Buffer!");
	cl_int pattern = 0;
	clEnqueueFillBuffer(queue, d_stBuffers, &pattern, sizeof(cl_int), 0, sizeof(unsigned int) * magconOffset, 0, NULL, NULL);

    //allocate d_infos on device and pin to host memory
	d_infos = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,  sizeof(CodeBlockAdditionalInfo) * codeBlocks, h_infos, &err);
    SAMPLE_CHECK_ERRORS(err);
    if (d_infos == (cl_mem)0)
        throw Error("Failed to create d_infos Buffer!");

	*coefficients = d_decodedCoefficientsBuffers;

}

float CoefficientCoder::decode(EntropyCodingTaskInfo *infos, int count, void** coefficients)
{
    cl_int err = CL_SUCCESS;

	int codeBlocks = count;
	int maxOutLength = MAX_CODESTREAM_SIZE;

	int argNum = 0;
	err = clSetKernelArg(myKernel, argNum++, sizeof(cl_mem),  &d_stBuffers);
    SAMPLE_CHECK_ERRORS(err);
	
	err = clSetKernelArg(myKernel, argNum++, sizeof(cl_mem), &d_codestreamBuffers);
    SAMPLE_CHECK_ERRORS(err);
	
	err = clSetKernelArg(myKernel, argNum++, sizeof(int),  &maxOutLength);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(myKernel, argNum++, sizeof(cl_mem), &d_infos);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(myKernel, argNum++, sizeof(int),  &codeBlocks);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(myKernel, argNum++, sizeof(cl_mem), &d_decodedCoefficientsBuffers);
    SAMPLE_CHECK_ERRORS(err);

	double t1 = time_stamp();
	const int THREADS = 16;
	int groups = (int) ceil((float) codeBlocks / THREADS);
	
	size_t global_work_size[1] = {groups * THREADS};
	size_t local_work_size[1] = {THREADS};
    // execute kernel
	err =  enqueue(1, global_work_size, local_work_size); 
    SAMPLE_CHECK_ERRORS(err);

	double t2 = time_stamp();
	return (t2 - t1)* 1000;

}

