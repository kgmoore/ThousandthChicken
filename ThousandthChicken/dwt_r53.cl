// License: please see LICENSE4 file for more details.
#include "dwt.cl"

 
// 5/3 forward DWT lifting schema coefficients
CONSTANT float reverse53Update = -0.25f;    /// undo 5/3 update
CONSTANT float reverse53Predict = 0.5f;  /// undo 5/3 predict


/// Horizontal 5/3 RDWT on specified lines of transform buffer.
/// @param lines      number of lines to be transformed
/// @param firstLine  index of the first line to be transformed
void horizontalTransform(LOCAL TransformBufferINT* transformBuffer,
                               const int lines, const int firstLine) {
    localMemoryFence();
    forEachHorizontalEven_Reverse53Update(transformBuffer,firstLine, lines);
    localMemoryFence();
    forEachHorizontalOdd_Reverse53Predict(transformBuffer,firstLine, lines);
    localMemoryFence();
}


 inline  void loadWindowIntoColumn(LOCAL TransformBufferINT* transformBuffer, GLOBAL const int * const input, RDWTColumn* col) {
      for(int i = 3; i < (3 + transformBuffer->WIN_SIZE_Y); i += 2) {
        transformBuffer->data[col->offset + i * transformBuffer->info.VERTICAL_STRIDE] = loadLowFromINT(&col->loader,input);
        transformBuffer->data[col->offset + (i + 1) * transformBuffer->info.VERTICAL_STRIDE] = loadHighFromINT(&col->loader,input);
      }
}





/// Initializes one column of shared transform buffer with 3 input pixels.
/// Those 3 pixels will not be transformed. Also initializes given loader.
/// @param columnX   x coordinate of column in shared transform buffer
/// @param input     input image
/// @param sizeX     width of the input image
/// @param sizeY     height of the input image
/// @param loader    (uninitialized) info about loaded column
void initRDWT53Column(LOCAL TransformBufferINT* transformBuffer, 
                  const int columnX, GLOBAL const int * const input, 
                  const int sizeX, const int sizeY,
                  RDWTColumn* column,
                  const int firstY) {
	LOCAL int* data = transformBuffer->data;

    // coordinates of the first coefficient to be loaded
    const int firstX = getGroupId(0) * transformBuffer->WIN_SIZE_X + columnX;

    // offset of the column with index 'colIndex' in the transform buffer
    column->offset = getColumnOffset(&transformBuffer->info,columnX);

	int STRIDE = transformBuffer->info.VERTICAL_STRIDE;

    if(getGroupId(1) == 0) {
		// topmost block - apply mirroring rules when loading first 3 rows
		initVerticalDWTBandLoader(&column->loader, column->CHECKED, sizeX, sizeY, firstX, firstY);

		// load pixels in mirrored way
		data[column->offset + 1 * STRIDE] = loadLowFromINT(&column->loader,input);
		data[column->offset + 0 * STRIDE] =
		data[column->offset + 2 * STRIDE] = loadHighFromINT(&column->loader,input);
    } else {
		// non-topmost row - regular loading:
		initVerticalDWTBandLoader(&column->loader, column->CHECKED, sizeX, sizeY, firstX, firstY - 1);
		data[column->offset + 0 * STRIDE] = loadHighFromINT(&column->loader,input);
		data[column->offset + 1 * STRIDE] = loadLowFromINT(&column->loader,input);
		data[column->offset + 2 * STRIDE] = loadHighFromINT(&column->loader,input);
    }
    // Now, the next coefficient, which will be loaded by loader, is #2.
}




/// Actual GPU 5/3 RDWT implementation.
/// @param CHECKED_LOADS   true if boundaries must be checked when reading
/// @param CHECKED_WRITES  true if boundaries must be checked when writing
/// @param in        input image (5/3 transformed coefficients)
/// @param out       output buffer (for reverse transformed image)
/// @param sizeX     width of the output image 
/// @param sizeY     height of the output image
/// @param winSteps  number of sliding window steps
void transform(LOCAL TransformBufferINT* transformBuffer, bool CHECKED_LOADS, bool CHECKED_WRITES,
                            GLOBAL const int * const in, GLOBAL int * const out,
                            const int sizeX, const int sizeY,
                            const int winSteps) {

    int WIN_SIZE_X = transformBuffer->WIN_SIZE_X;
	int WIN_SIZE_Y = transformBuffer->WIN_SIZE_Y;
	int STRIDE = transformBuffer->info.VERTICAL_STRIDE;
	LOCAL int* data = transformBuffer->data;

    // info about one main and one boundary column
    RDWTColumn column, boundaryColumn;
	initRDWTColumn(&column, CHECKED_LOADS);
	initRDWTColumn(&boundaryColumn, CHECKED_LOADS);

    // index of first row to be transformed
    const int firstY = getGroupId(1) * WIN_SIZE_Y * winSteps;

    // some threads initialize boundary columns
    clearRDWTColumn(&boundaryColumn);
    if(getLocalId(0) < 3) {
		// First 3 threads also handle boundary columns. Thread #0 gets right
		// column #0, thread #1 get right column #1 and thread #2 left column.
		const int colId = getLocalId(0) + ((getLocalId(0) != 2) ? WIN_SIZE_X : -3);

		// Thread initializes offset of the boundary column (in LOCAL 
		// buffer), first 3 pixels of the column and a loader for this column.
		initRDWT53Column(transformBuffer, colId, in, sizeX, sizeY, &boundaryColumn, firstY);
    }

    // All threads initialize central columns.
    initRDWT53Column(transformBuffer,parityIdx(transformBuffer->WIN_SIZE_X), in, sizeX, sizeY, &column, firstY);

    // horizontally transform first 3 rows
    horizontalTransform(transformBuffer,3, 0);

    // writer of output pixels - initialize it
    const int outX = getGroupId(0) * WIN_SIZE_X + getLocalId(0);
	VerticalDWTPixelWriter writer;
	initVerticalDWTPixelWriter(&writer, CHECKED_WRITES, sizeX, sizeY, outX, firstY);

    // offset of column (in transform buffer) saved by this thread
    const int outputColumnOffset = getColumnOffset(&transformBuffer->info, getLocalId(0));

    // (Each iteration assumes that first 3 rows of transform buffer are
    // already loaded with horizontally transformed pixels.)
    for(int w = 0; w < winSteps; w++) {
		// Load another WIN_SIZE_Y lines of this thread's column
		// into the transform buffer.
		loadWindowIntoColumn(transformBuffer,in, &column);

		// possibly load boundary columns
		if(getLocalId(0) < 3) {
			loadWindowIntoColumn(transformBuffer, in, &boundaryColumn);
		}

		// horizontally transform all newly loaded lines
		horizontalTransform(transformBuffer,WIN_SIZE_Y, 3);

		// Using 3 registers, remember current values of last 3 rows 
		// of transform buffer. These rows are transformed horizontally 
		// only and will be used in next iteration.
		int last3Lines[3];
		last3Lines[0] = data[outputColumnOffset + (WIN_SIZE_Y + 0) * STRIDE];
		last3Lines[1] = data[outputColumnOffset + (WIN_SIZE_Y + 1) * STRIDE];
		last3Lines[2] = data[outputColumnOffset + (WIN_SIZE_Y + 2) * STRIDE];

		// vertically transform all central columns
		forEachVerticalOdd_Reverse53Update(transformBuffer, outputColumnOffset);
		forEachVerticalEven_Reverse53Predict(transformBuffer, outputColumnOffset);

		// Save all results of current window. Results are in transform buffer
		// at rows from #1 to #(1 + WIN_SIZE_Y). Other rows are invalid now.
		// (They only served as a boundary for vertical RDWT.)
		for(int i = 1; i < (1 + WIN_SIZE_Y); i++) {
			writeIntoINT(&writer, out, data[outputColumnOffset + i * STRIDE]);
		}

		// Use last 3 remembered lines as first 3 lines for next iteration.
		// As expected, these lines are already horizontally transformed.
		data[outputColumnOffset + 0 * STRIDE] = last3Lines[0];
		data[outputColumnOffset + 1 * STRIDE] = last3Lines[1];
		data[outputColumnOffset + 2 * STRIDE] = last3Lines[2];

		// Wait for all writing threads before proceeding to loading new
		// coefficients in next iteration. (Not to overwrite those which
		// are not written yet.)
		localMemoryFence();
    }
}


/// Main GPU 5/3 RDWT entry point.
/// @param in     input image (5/3 transformed coefficients)
/// @param out    output buffer (for reverse transformed image)
/// @param sizeX  width of the output image 
/// @param sizeY  height of the output image
/// @param winSteps  number of sliding window steps
KERNEL void run(int WIN_SIZE_X, int WIN_SIZE_Y, LOCAL int* data,
                            GLOBAL const int * const input, GLOBAL int * const output,
                            const int sx, const int sy, const int steps) {
    // prepare instance with buffer in shared memory
    LOCAL TransformBufferINT transformBuffer;
	initTransformBufferINT(&transformBuffer, WIN_SIZE_X, WIN_SIZE_Y, data);


    // Compute limits of this threadblock's block of pixels and use them to
    // determine, whether this threadblock will have to deal with boundary.
    // (1 in next expressions is for radius of impulse response of 5/3 RDWT.)
    const int maxX = (getGroupId(0) + 1) * WIN_SIZE_X + 1;
    const int maxY = (getGroupId(1) + 1) * WIN_SIZE_Y * steps + 1;
    const bool atRightBoundary = maxX >= sx;
    const bool atBottomBoundary = maxY >= sy;

    // Select specialized version of code according to distance of this
    // threadblock's pixels from image boundary.
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

    