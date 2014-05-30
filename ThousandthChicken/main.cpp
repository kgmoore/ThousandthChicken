#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"

using namespace cv;

#include "ocl_util.h"

#include "DWTForward53.h"
#include "DWTReverse53.h"
#include "DWTForward97.h"
#include "DWTReverse97.h"

#include "DWTKernel.cpp"


#define OCL_SAMPLE_IMAGE_NAME "baboon.png"

extern bool quiet;

// Parse the command line arguments and return the found values in the data structure
// The valid arguments are:
//      -cpu: Prefer a CPU OpenCL device         - Set preferCpu to true
//      -gpu: Prefer a GPU OpenCL device         - Set preferGpu to true
//      -q:   Set global variable quite to true
int ParseArguments(data_args_d_t* data, int argc, char* argv[])
{
    data->preferCpu      = data->preferGpu = false;
    data->vendorName     = NULL;
    cl_int errorCode = CL_SUCCESS;

    for (int i = 1; i < argc ; i++)
    {
        if (!strcmp(argv[i], "-cpu"))
        {
            data->preferCpu = true;
        }
        else if (!strcmp(argv[i], "-q"))
        {
            quiet = true;
        }
        else if (!strcmp(argv[i], "-gpu"))
        {
            data->preferGpu = true;
        }
        else if (!strcmp(argv[i], "-help"))
        {
            LogInfo(
                "Usage: ocl_library_samples.exe\n"
                "                       [options]\n"
                "Options:\n"
                "      -cpu: Prefer a CPU OpenCL device\n"
                "      -gpu: Prefer a GPU OpenCL device\n"
                "      -help: print command options\n"
                "      -i: Print device info\n"
                "      -q: Run in silence mode\n"
                );
        }
        else
        {
            errorCode = CL_OUT_OF_RESOURCES;
        }
    }

    return errorCode;
}


// Read and normalize the input image
Mat ReadInputImage(const std::string &fileName, int flag, int alignCols, int alignRows)
{
    Size dsize;
    Mat img = imread(fileName, flag);

    if (! img.empty())
    {
        // Make sure that the input image size is fit:
        // number of rows is multiple of 8
        // number of columns is multiple of 64
        dsize.height = ((img.rows % alignRows) == 0) ? img.rows : (((img.rows + alignRows - 1) / alignRows) * alignRows);
        dsize.width = ((img.cols % alignCols) == 0) ? img.cols : (((img.cols + alignCols - 1) / alignCols) * alignCols);
        resize(img, img, dsize);
    }

    return img;
}

int main(int argc, char* argv[])
{
    data_args_d_t args;
     // Parse command line arguments
    int error_code = ParseArguments(&args, argc, argv);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: ParseArguments returned %s.\n", TranslateOpenCLError(error_code));
        return error_code;
    }

    // Based on args values, initialze the OpenCL environment:
    // find OpenCL platform and device and create OpenCL context and command-queue
    // The OpenCL parameters are returned in ocl
    ocl_args_d_t ocl;
    error_code = InitOpenCL(&ocl, &args);
    if (CL_SUCCESS != error_code)
    {
        LogError("Error: InitOpenCL returned %s.\n", TranslateOpenCLError(error_code));
        return error_code;
    }

    // Read the input image
    Mat img_src = ReadInputImage(OCL_SAMPLE_IMAGE_NAME, CV_8UC1, 8, 64);
    if (img_src.empty())
    {
        LogError("Cannot read image file: %s\n", OCL_SAMPLE_IMAGE_NAME);
        return error_code;
    }

    Mat img_dst = Mat::zeros(img_src.size(), CV_8UC1);

	
	int imageSize = img_src.cols * img_src.rows;

	 imshow("Before:", img_src);
     waitKey();
	const bool isLossy = false;
	const bool writeOutput = false;
	if (isLossy) {
		DWTForward97* fdwt97 = new DWTForward97(DwtKernelInitInfo(ocl.commandQueue, "-g -s \"c:\\src\\ThousandthChicken\\ThousandthChicken\\dwt_f97.cl\""));
		DWTReverse97* rdwt97 = new DWTReverse97(DwtKernelInitInfo(ocl.commandQueue, "-g -s \"c:\\src\\ThousandthChicken\\ThousandthChicken\\dwt_r97.cl\""));

		float* input = new float[imageSize];
		for (int i = 0; i < imageSize; ++i) {
			input[i] = (img_src.ptr()[i]/255.0f) - 0.5f;  // normalize to [-0.5, +0.5] range
		}
		

		fdwt97->run(input, img_src.cols, img_src.rows, 1);
		float* forward = fdwt97->mapOutputBufferToHost();
		rdwt97->run(forward, img_src.cols, img_src.rows, 1);
		float* reverse = rdwt97->mapOutputBufferToHost();
		for (int i = 0; i < imageSize; ++i)
			img_dst.ptr()[i] =  (uchar)((reverse[i] + 0.5f) * 255.0f); // convert to [0, 255] range

		if (writeOutput)
		   imwrite("c:\\tmp\\baboon53.png", img_dst);


		delete fdwt97;
		delete[] input;

	} else {
		
		DWTForward53* fdwt53 = new DWTForward53( DwtKernelInitInfo(ocl.commandQueue, "-g -s \"c:\\src\\ThousandthChicken\\ThousandthChicken\\dwt_f53.cl\""));
		DWTReverse53* rdwt53 = new DWTReverse53( DwtKernelInitInfo(ocl.commandQueue, "-g -s \"c:\\src\\ThousandthChicken\\ThousandthChicken\\dwt_r53.cl\""));


		int* input = new int[imageSize];
		for (int i = 0; i < imageSize; ++i) {
			input[i] = img_src.ptr()[i] - 128;  // normalize to [-128,127] range
		}

		fdwt53->run(input, img_src.cols, img_src.rows, 1);
		int* forward = fdwt53->mapOutputBufferToHost();
		//rdwt53->run(forward, img_src.cols, img_src.rows, 1);
		//int* reverse = rdwt53->mapOutputBufferToHost();
		for (int i = 0; i < imageSize; ++i)
			//img_dst.ptr()[i] =  input[i] == reverse[i] ? 0 : 255;  // do diff with input image
			img_dst.ptr()[i] =  forward[i] + 128;
				
		if (writeOutput)
		   imwrite("c:\\tmp\\baboon53.png", img_dst);

		delete rdwt53;
		delete fdwt53;
		delete[] input;

	}
    imshow("After:", img_dst);
    waitKey();

    LogInfo("Done.\n");

    return 0;
}

