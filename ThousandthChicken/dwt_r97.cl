#include "dwt.cl"

// 9/7 reverse DWT lifting schema coefficients
CONSTANT float r97update2 = -0.4435068522;    ///< undo 9/7 update 2
CONSTANT float r97predict2 = -0.8829110762;  ///< undo 9/7 predict 2
CONSTANT float r97update1 = 0.05298011854;    ///< undo 9/7 update 1
CONSTANT float r97Predict1 = 1.586134342;  ///< undo 9/7 predict 1

 inline  void loadWindowIntoColumn(LOCAL TransformBufferFLOAT* transformBuffer, GLOBAL const float * const input, RDWTColumn* col) {
	int WIN_SIZE_Y = transformBuffer->WIN_SIZE_Y;
	int STRIDE = transformBuffer->info.VERTICAL_STRIDE;
	LOCAL float* data = transformBuffer->data;
	uint index1 = col->offset + 7 * STRIDE;
	uint index2 = index1 + STRIDE;
    for(int i = 7; i < (7 + WIN_SIZE_Y); i+=2) {

         data[index1] = loadLowFromFLOAT(&col->loader,input);
		 data[index2] = loadHighFromFLOAT(&col->loader,input);
		 index1 = index2 + STRIDE;
		 index2 = index1 + STRIDE;
    }
}


/// Horizontal 9/7 RDWT on specified lines of transform buffer.
/// @param lines      number of lines to be transformed
/// @param firstLine  index of the first line to be transformed
void horizontalRDWT97(LOCAL TransformBufferFLOAT* transformBuffer, const int lines, const int firstLine) {

		//AB removed to make image appear correctly
		//localMemoryFence();
		//scaleHorizontal(transformBuffer,scale97Mul, scale97Div, firstLine, lines);
		localMemoryFence();
		forEachHorizontalEvenScale(transformBuffer,firstLine, lines, r97update2);
		localMemoryFence();
		forEachHorizontalOddScale(transformBuffer,firstLine, lines, r97predict2);
		localMemoryFence();
		forEachHorizontalEvenScale(transformBuffer,firstLine, lines, r97update1);
		localMemoryFence();
		forEachHorizontalOddScale(transformBuffer,firstLine, lines, r97Predict1);
		localMemoryFence();
		localMemoryFence();
}



/// Initializes one column: computes offset of the column in local memory
/// buffer, initializes loader and finally uses it to load first 7 pixels.
/// @param columnX  x-axis coordinate of the column (relative to the left
///                  side of this threadblock's block of input pixels)
/// @param input     input image
/// @param sizeX     width of the input image
/// @param sizeY     height of the input image
/// @param column    (uninitialized) column info to be initialized
/// @param firstY    y-axis coordinate of first image row to be transformed
void initRDWT97Column(LOCAL TransformBufferFLOAT* transformBuffer, 
                  const int columnX, GLOBAL const float * const input, 
                  const int sizeX, const int sizeY,
                  RDWTColumn* column,
                  const int firstY) {

	LOCAL float* data = transformBuffer->data;

   // coordinates of the first coefficient to be loaded
    const int firstX = getGroupId(0) * transformBuffer->WIN_SIZE_X + columnX;

    // offset of the column with index 'colIndex' in the transform buffer
    column->offset = getColumnOffset(&transformBuffer->info,columnX);

	int STRIDE = transformBuffer->info.VERTICAL_STRIDE;

    if(getGroupId(1) == 0) {
		// topmost block - apply mirroring rules when loading first 7 rows
		initVerticalDWTBandLoader(&column->loader, column->CHECKED, sizeX, sizeY, firstX, firstY);

		// load pixels in mirrored way
        data[column->offset + 3 * STRIDE] = loadLowFromFLOAT(&column->loader,input);
        data[column->offset + 4 * STRIDE] =
        data[column->offset + 2 * STRIDE] = loadHighFromFLOAT(&column->loader,input);
        data[column->offset + 5 * STRIDE] =
		data[column->offset + 1 * STRIDE] = loadLowFromFLOAT(&column->loader,input);
        data[column->offset + 6 * STRIDE] = 
        data[column->offset + 0 * STRIDE] = loadHighFromFLOAT(&column->loader,input);

    } else {

		// non-topmost row - regular loading:
		initVerticalDWTBandLoader(&column->loader, column->CHECKED, sizeX, sizeY, firstX, firstY - 3);
		
	   // load 7 rows into the transform buffer
        for(int i = 0; i < 7; i++) {
		  if ( (i & 1) == 0)
            data[column->offset + i * STRIDE] = loadHighFromFLOAT(&column->loader,input);
		  else
		   data[column->offset + i * STRIDE] = loadLowFromFLOAT(&column->loader,input);
        }
    }
   // Now, the next pixel, which will be loaded by loader, is pixel #4.
}


/// Actual GPU 9/7 RDWT implementation.
/// @param CHECKED_LOADS   true if boundaries must be checked when reading
/// @param CHECKED_WRITES  true if boundaries must be checked when writing
/// @param in        input image (untransformed image)
/// @param out       output buffer (9/7 transformed coefficients) 
/// @param sizeX     width of the output image 
/// @param sizeY     height of the output image
/// @param winSteps  number of sliding window steps
void transform(LOCAL TransformBufferFLOAT* transformBuffer, bool CHECKED_LOADS, bool CHECKED_WRITES,
                            GLOBAL const float * const in, GLOBAL float * const out,
                            const int sizeX, const int sizeY,
                            const int winSteps) {

    int WIN_SIZE_X = transformBuffer->WIN_SIZE_X;
	int WIN_SIZE_Y = transformBuffer->WIN_SIZE_Y;
	int STRIDE = transformBuffer->info.VERTICAL_STRIDE;

	LOCAL float* data = transformBuffer->data;

	// info about one main and one boundary columns processed by this thread
    RDWTColumn loadedColumn;    
    RDWTColumn boundaryColumn;  // only few threads use this
	initRDWTColumn(&loadedColumn, CHECKED_LOADS);

    // Initialize all column info: initialize loaders, compute offset of 
    // column in shared buffer and initialize loader of column.
    const int firstY = getGroupId(1) * WIN_SIZE_Y * winSteps;
    initRDWT97Column(transformBuffer, getLocalId(0), in, sizeX, sizeY, &loadedColumn, firstY);


    // first 7 threads initialize boundary columns, others do not use them
    if(getLocalId(0)< 7) {
		// index of boundary column (relative x-axis coordinate of the column)
		const int colId = getLocalId(0) + ((getLocalId(0) < 3) ? WIN_SIZE_X : -7);

		// initialize the column
		clearRDWTColumn(&boundaryColumn);
		initRDWTColumn(&boundaryColumn, CHECKED_LOADS);

	   // work item initializes offset of the boundary column (in LOCAL buffer),
        // first 7 pixels of the column and a loader for this column.
		initRDWT97Column(transformBuffer, colId, in, sizeX, sizeY, &boundaryColumn, firstY);
    }

	// horizontally transform first 7 rows in all columns
     horizontalRDWT97(transformBuffer, 7, 0);

    // index of column which will be written into output by this thread
    const int outColumnIndex = parityIdx(WIN_SIZE_X);


    // offset of column which will be written by this thread into output
    const int outColumnOffset = getColumnOffset(&transformBuffer->info, outColumnIndex);

    // initialize output writer for this thread
    const int outputFirstX = getGroupId(0) * WIN_SIZE_X + outColumnIndex;

    VerticalDWTPixelWriter writer;
    initVerticalDWTPixelWriter(&writer, CHECKED_WRITES, sizeX, sizeY, outputFirstX, firstY);


      // (Each iteration of this loop assumes that first 7 rows of transform 
      // buffer are already loaded with horizontally transformed coefficients.)
    for(int w = 0; w < winSteps; w++) {
	     // Load another WIN_SIZE_Y lines of thread's column into the buffer.
        loadWindowIntoColumn(transformBuffer,in, &loadedColumn);

        // some work items also load boundary columns
        if(getLocalId(0) < 7) {
          loadWindowIntoColumn(transformBuffer,in, &boundaryColumn);
        }

		// horizontally transform all newly loaded lines
        horizontalRDWT97(transformBuffer, WIN_SIZE_Y, 7);

     	// Using 7 registers, remember current values of last 7 rows of
        // transform buffer. These rows are transformed horizontally only 
        // and will be used in next iteration.
        float last7Lines[7];
		uint index = outColumnOffset + WIN_SIZE_Y * STRIDE;
        for(int i = 0; i < 7; i++) {
          last7Lines[i] = data[index];
		  index += STRIDE;
        }

		 // vertically transform all central columns
        scaleVertical(transformBuffer, scale97Div, scale97Mul, outColumnOffset, WIN_SIZE_Y + 7, 0);
        forEachVerticalOddScale(transformBuffer,outColumnOffset, r97update2);
        forEachVerticalEvenScale(transformBuffer,outColumnOffset, r97predict2);
        forEachVerticalOddScale(transformBuffer,outColumnOffset, r97update1);
        forEachVerticalEvenScale(transformBuffer,outColumnOffset, r97Predict1);


        // Save all results of current window. Results are in transform buffer
        // at rows from #3 to #(3 + WIN_SIZE_Y). Other rows are invalid now.
        // (They only served as a boundary for vertical RDWT.)
		index = outColumnOffset + 3 * STRIDE;
		for(int i = 3; i < (3 + WIN_SIZE_Y); i++) {
			writeIntoFLOAT(&writer, out, data[index]);
			index += STRIDE;
		}

		
        // Use last 7 remembered lines as first 7 lines for next iteration.
        // As expected, these lines are already horizontally transformed.
		 index = outColumnOffset;
        for(int i = 0; i < 7; i++) {
          data[index] = last7Lines[i];
		  index += STRIDE;
        }
		
	
		// before proceeding to next iteration, wait for all output columns
		// to be written into the output
		localMemoryFence();

	}
}


/// Main GPU 9/7 FDWT entry point.
/// @param in     input image ( untransformed image)
/// @param out    output buffer (9/7 transformed coefficients)
/// @param sizeX  width of the output image 
/// @param sizeY  height of the output image
/// @param winSteps  number of sliding window steps
KERNEL void run(int WIN_SIZE_X, int WIN_SIZE_Y, LOCAL float* data,
                            GLOBAL const float * const input, GLOBAL float * const output,
                            const int sx, const int sy, const int steps) {
    // prepare instance with buffer in shared memory
    LOCAL TransformBufferFLOAT transformBuffer;
	initTransformBufferFLOAT(&transformBuffer, WIN_SIZE_X, WIN_SIZE_Y, data);

    // Compute limits of this workgroup's block of pixels and use them to
    // determine, whether this workgroup will have to deal with boundary.
    // (3 in next expressions is for radius of impulse response of 9/7 RDWT.)
    const int maxX = (getGroupId(0) + 1) * WIN_SIZE_X + 3;
    const int maxY = (getGroupId(1) + 1) * WIN_SIZE_Y * steps + 3;
    const bool atRightBoundary = maxX >= sx;
    const bool atBottomBoundary = maxY >= sy;

    // Select specialized version of code according to distance of this
    // workroup's pixels from image boundary.
    if(atBottomBoundary) {
		// near bottom boundary => check both writing and reading
		transform(&transformBuffer, true, true,input, output, sx, sy, steps);
    } else if(atRightBoundary) {
		// near right boundary only => check writing only
		transform(&transformBuffer,false, true,input, output, sx, sy, steps);
    } else {
		// no nearby boundary => check nothing
		transform(&transformBuffer,false, false,input, output, sx, sy, steps);
    }
}


