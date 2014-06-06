// License: please see LICENSE1 file for more details.

#include "ocl_util.h"

#include "DWTTest.h"
#include "Decoder.h"

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

	Decoder decoder(&ocl);
	decoder.decode("c:\\src\\openjpeg-data\\input\\conformance\\file1.jp2");

//	DWTTest dwtTester;
//	dwtTester.test(&ocl);




    LogInfo("Done.\n");

    return 0;
}

