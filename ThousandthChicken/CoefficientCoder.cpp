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


CoefficientCoder::CoefficientCoder(KernelInitInfoBase initInfo) : GenericKernel( KernelInitInfo(initInfo, "coefficient_coder.cl", "g_decode") )
{
}


CoefficientCoder::~CoefficientCoder(void)
{
}


void CoefficientCoder::decode_tile(type_tile *tile)
{
//	println_start(INFO);

//	start_measure();

	std::list<type_codeblock *> cblks;
	extract_cblks(tile, cblks);

	EntropyCodingTaskInfo *tasks = (EntropyCodingTaskInfo *) calloc( cblks.size(),sizeof(EntropyCodingTaskInfo));

	std::list<type_codeblock *>::iterator ii = cblks.begin();

	int num_tasks = 0;
	for(; ii != cblks.end(); ++ii)
	{
		convert_to_decoding_task(tasks[num_tasks++], *(*ii));
	}

//	printf("%d\n", num_tasks);

	float t = gpuDecode(tasks, num_tasks, &tile->coefficients);

	printf("kernel consumption: %f\n", t);
	free(tasks);

//	stop_measure(INFO);

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
void CoefficientCoder::convert_to_decoding_task(EntropyCodingTaskInfo &task, type_codeblock &cblk)
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

	cblk.d_coefficientsOffset = task.nominalWidth * task.nominalHeight;

	task.magbits = cblk.parent_sb->mag_bits;

	task.codeStream = cblk.codestream;
	task.length = cblk.length;
	task.significantBits = cblk.significant_bits;
}

float CoefficientCoder::gpuDecode(EntropyCodingTaskInfo *infos, int count, void** coefficients)
{

    LARGE_INTEGER perf_freq;
    LARGE_INTEGER perf_start;
    LARGE_INTEGER perf_stop;

    cl_int err = CL_SUCCESS;

	int codeBlocks = count;
	int maxOutLength = MAX_CODESTREAM_SIZE;

	cl_device_id device = NULL;
	cl_context context  = NULL;

    // Obtaing the OpenCL context from the command-queue properties
	err = clGetCommandQueueInfo(queue, CL_QUEUE_CONTEXT, sizeof(cl_context), &context, NULL);
	if (CL_SUCCESS != err)
	{
		LogError("Error: clGetCommandQueueInfo (CL_QUEUE_CONTEXT) returned %s.\n", TranslateOpenCLError(err));
		return 0;
	}

    // Obtain the OpenCL device from the command-queue properties
	err = clGetCommandQueueInfo(queue, CL_QUEUE_DEVICE, sizeof(cl_device_id), &device, NULL);
	if (CL_SUCCESS != err)
	{
		LogError("Error: clGetCommandQueueInfo (CL_QUEUE_DEVICE) returned %s.\n", TranslateOpenCLError(err));
		return 0;
	}



    // execute kernel
	cl_uint dev_alignment = requiredOpenCLAlignment(device);

	// allocate codestream buffer on host
	unsigned char* h_codestreamBuffers = (unsigned char*)aligned_malloc(sizeof(unsigned char) * codeBlocks * maxOutLength,dev_alignment);
    if (h_codestreamBuffers == NULL)
        throw Error("Failed to create h_codestreamBuffer Buffer!");


	// allocate h_infos on host
	CodeBlockAdditionalInfo *h_infos = (CodeBlockAdditionalInfo *) aligned_malloc(sizeof(CodeBlockAdditionalInfo) * codeBlocks,dev_alignment);
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
		h_infos[i].stripeNo = (int) ceil(infos[i].height / 4.0f);
		h_infos[i].subband = infos[i].subband;
		h_infos[i].magconOffset = magconOffset + infos[i].width;
		h_infos[i].magbits = infos[i].magbits;
		h_infos[i].length = infos[i].length;
		h_infos[i].significantBits = infos[i].significantBits;
		h_infos[i].d_coefficientsOffset = coefficientsOffset;
		coefficientsOffset +=  infos[i].nominalWidth * infos[i].nominalHeight;

	    //copy each code block codeStream buffer to host memory block
		memcpy(infos[i].codeStream, (void *) (h_codestreamBuffers + i * maxOutLength), sizeof(unsigned char) * infos[i].length);

		magconOffset += h_infos[i].width * (h_infos[i].stripeNo + 2);
	}

	//allocate d_coefficients on device
	cl_mem d_decodedCoefficientsBuffers = clCreateBuffer(context, CL_MEM_READ_WRITE ,  sizeof(int) * coefficientsOffset, NULL, &err);
    SAMPLE_CHECK_ERRORS(err);
    if (d_decodedCoefficientsBuffers == (cl_mem)0)
        throw Error("Failed to create d_decodedCoefficientsBuffers Buffer!");

	//allocate d_codestreamBuffer on device and pin to host memory
	cl_mem d_codestreamBuffers = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR, sizeof(unsigned char) * codeBlocks * maxOutLength, h_codestreamBuffers, &err);
    SAMPLE_CHECK_ERRORS(err);
    if (d_codestreamBuffers == (cl_mem)0)
        throw Error("Failed to create d_codestreamBuffers Buffer!");

	//allocate d_stBuffers on device and initialize it to zero
	cl_mem d_stBuffers = clCreateBuffer(context, CL_MEM_READ_WRITE ,  sizeof(unsigned int) * magconOffset, NULL, &err);
    SAMPLE_CHECK_ERRORS(err);
    if (d_stBuffers == (cl_mem)0)
        throw Error("Failed to create d_infos Buffer!");
	cl_int pattern = 0;
	clEnqueueFillBuffer(queue, d_stBuffers, &pattern, sizeof(cl_int), 0, sizeof(unsigned int) * magconOffset, 0, NULL, NULL);

    //allocate d_infos on device and pin to host memory
	cl_mem d_infos = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,  sizeof(CodeBlockAdditionalInfo) * codeBlocks, h_infos, &err);
    SAMPLE_CHECK_ERRORS(err);
    if (d_infos == (cl_mem)0)
        throw Error("Failed to create d_infos Buffer!");


	/////////////////////////////////////////////////////////////////////////////
	// set kernel arguments ///////////////////////////////
	
	err = clSetKernelArg(myKernel, 0, sizeof(cl_mem), (void *) &d_stBuffers);
    SAMPLE_CHECK_ERRORS(err);
	
	err = clSetKernelArg(myKernel, 1, sizeof(cl_mem), (void *) &d_codestreamBuffers);
    SAMPLE_CHECK_ERRORS(err);
	
	err = clSetKernelArg(myKernel, 2, sizeof(int), (void *) &maxOutLength);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(myKernel, 3, sizeof(cl_mem), (void *) &d_infos);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(myKernel, 4, sizeof(int), (void *) &codeBlocks);
    SAMPLE_CHECK_ERRORS(err);

	err = clSetKernelArg(myKernel, 5, sizeof(cl_mem), (void *) &d_decodedCoefficientsBuffers);
    SAMPLE_CHECK_ERRORS(err);

	//////////////////////////////////////////////////////////////////////////////////
		
	/////////////////////////////
	// execute kernel
	QueryPerformanceCounter(&perf_start);
	const int THREADS = 32;
	int groups = (int) ceil((float) codeBlocks / THREADS);
	
	size_t global_work_size[1] = {groups * THREADS};
	size_t local_work_size[1] = {THREADS};
    // execute kernel
	err = clEnqueueNDRangeKernel(queue, myKernel, 1, NULL, global_work_size, local_work_size, 0, NULL, NULL);
    SAMPLE_CHECK_ERRORS(err);

	err = clFinish(queue);
    SAMPLE_CHECK_ERRORS(err);
    QueryPerformanceCounter(&perf_stop);

	/*
	//////////////////////////////
	// read memory back into host from decodedCoefficientsBuffer on device 
	int* tmp_ptr = NULL;
    tmp_ptr = (int*)clEnqueueMapBuffer(oclObjects.queue, d_decodedCoefficientsBuffers, true, CL_MAP_READ, 0,  sizeof(int) * coefficientsOffset, 0, NULL, NULL, NULL);
    err = clFinish(oclObjects.queue);
    SAMPLE_CHECK_ERRORS(err);

	int* decodedCoefficientsBuffer = tmp_ptr;
	coefficientsOffset = 0;
	for(int i = 0; i < codeBlocks; i++)
	{
		int coeffecientsBufferSize = infos[i].nominalWidth * infos[i].nominalHeight;
		infos[i].coefficients = (int*)malloc(coeffecientsBufferSize *sizeof(int));

		if (infos[i].significantBits > 0)
			memcpy(infos[i].coefficients, decodedCoefficientsBuffer, coeffecientsBufferSize * sizeof(int));
		else
			memset(infos[i].coefficients,  0, coeffecientsBufferSize * sizeof(int));
      decodedBuffer +=  coeffecientsBufferSize;
	}

    err = clEnqueueUnmapMemObject(oclObjects.queue, d_decodedCoefficientsBuffers, tmp_ptr, 0, NULL, NULL);
    SAMPLE_CHECK_ERRORS(err);
	*/

    //////////////////////////////////////////
    //release memory

	 err = clReleaseMemObject(d_infos);
    SAMPLE_CHECK_ERRORS(err);

	err = clReleaseMemObject(d_stBuffers);
    SAMPLE_CHECK_ERRORS(err);
		
	err = clReleaseMemObject(d_codestreamBuffers);
    SAMPLE_CHECK_ERRORS(err);

	*coefficients = d_decodedCoefficientsBuffers;

	aligned_free(h_infos);

	aligned_free(h_codestreamBuffers);


	// retrieve perf. counter frequency
	QueryPerformanceCounter(&perf_stop);
    QueryPerformanceFrequency(&perf_freq);
    float rc =  (float)(perf_stop.QuadPart - perf_start.QuadPart)/(float)perf_freq.QuadPart;

	return rc;

}

