#include "dwt.cl"


// 5/3 forward DWT lifting schema coefficients
CONSTANT float forward53Predict = -0.5f;   /// forward 5/3 predict
CONSTANT float forward53Update = 0.25f;    /// forward 5/3 update

/// Initializes one column: computes offset of the column in local memory
/// buffer, initializes loader and finally uses it to load first 3 pixels.
/// @param columnX  x-axis coordinate of the column (relative to the left
///                  side of this threadblock's block of input pixels)
/// @param input     input image
/// @param sizeX     width of the input image
/// @param sizeY     height of the input image
/// @param column    (uninitialized) column info to be initialized
/// @param firstY    y-axis coordinate of first image row to be transformed
void initFDWT53Column(LOCAL TransformBufferINT* transformBuffer, 
                  const int columnX, GLOBAL const int * const input, 
                  const int sizeX, const int sizeY,
                  FDWTColumn* column,
                  const int firstY) {
    // coordinates of the first coefficient to be loaded
    const int firstX = getGroupId(0) * transformBuffer->WIN_SIZE_X + columnX;

    // offset of the column with index 'colIndex' in the transform buffer
    column->offset = getColumnOffset(&transformBuffer->info,columnX);

    if(getGroupId(1) == 0) {
		// topmost block - apply mirroring rules when loading first 3 rows
		initVerticalDWTPixelLoader(&column->loader, column->CHECKED, sizeX, sizeY, firstX, firstY);

		column->pixel0 = loadFromINT(&column->loader,input);
		column->pixel1 = loadFromINT(&column->loader,input);
		column->pixel2 = loadFromINT(&column->loader,input);

		 // reinitialize loader to start with pixel #1 again
		initVerticalDWTPixelLoader(&column->loader, column->CHECKED, sizeX, sizeY, firstX, firstY+1);

    } else {
		// non-topmost row - regular loading:
		initVerticalDWTPixelLoader(&column->loader, column->CHECKED, sizeX, sizeY, firstX, firstY - 2);

		// load 3 rows into the column
		column->pixel0 = loadFromINT(&column->loader,input);
		column->pixel1 = loadFromINT(&column->loader,input);
		column->pixel2 = loadFromINT(&column->loader,input);

		  // Now, the next pixel, which will be loaded by loader, is pixel #1.
    }
}

/// Loads and vertically transforms given column. Assumes that first 3
/// pixels are already loaded in column fields pixel0 ... pixel2.
/// @param CHECKED  true if loader of the column checks boundaries
/// @param column    column to be loaded and vertically transformed
/// @param input     pointer to input image data
void loadAndVerticallyTransform(LOCAL TransformBufferINT* transformBuffer, 
                                             FDWTColumn* column,
                                               GLOBAL const int * const input) {
											   
    int WIN_SIZE_X = transformBuffer->WIN_SIZE_X;
	int WIN_SIZE_Y = transformBuffer->WIN_SIZE_Y;
	int STRIDE = transformBuffer->info.VERTICAL_STRIDE;
	LOCAL int* data = transformBuffer->data;

    // take 3 loaded pixels and put them into shared memory transform buffer
    data[column->offset + 0 * STRIDE] = column->pixel0;
    data[column->offset + 1 * STRIDE] = column->pixel1;
    data[column->offset + 2 * STRIDE] = column->pixel2;

    // load remaining pixels to be able to vertically transform the window
    for(int i = 3; i < (3 + WIN_SIZE_Y); i++) {
		 data[column->offset + i * STRIDE] = loadFromINT(&column->loader,input);
    }

    // remember last 3 pixels for use in next iteration
    column->pixel0 = data[column->offset + (WIN_SIZE_Y + 0) * STRIDE];
    column->pixel1 = data[column->offset + (WIN_SIZE_Y + 1) * STRIDE];
    column->pixel2 = data[column->offset + (WIN_SIZE_Y + 2) * STRIDE];

	forEachVerticalOdd_Forward53Predict(transformBuffer,column->offset);
    forEachVerticalEven_Forward53Update(transformBuffer,column->offset);
}


/// Actual GPU 5/3 FDWT implementation.
/// @param CHECKED_LOADS   true if boundaries must be checked when reading
/// @param CHECKED_WRITES  true if boundaries must be checked when writing
/// @param in        input image (reverse transformed image)
/// @param out       output buffer (5/3 transformed coefficients) 
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

	// info about one main and one boundary columns processed by this thread
    FDWTColumn column;    
    FDWTColumn boundaryColumn;  // only few threads use this
	initFDWTColumn(&column, CHECKED_LOADS);



    // Initialize all column info: initialize loaders, compute offset of 
    // column in shared buffer and initialize loader of column.
    const int firstY = getGroupId(1) * WIN_SIZE_Y * winSteps;
    initFDWT53Column(transformBuffer, getLocalId(0), in, sizeX, sizeY, &column, firstY);

    // first 3 threads initialize boundary columns, others do not use them
    if(getLocalId(0)< 3) {
		// index of boundary column (relative x-axis coordinate of the column)
		const int colId = getLocalId(0) + ((getLocalId(0) == 0) ? WIN_SIZE_X : -3);

		// initialize the column
		clearFDWTColumn(&boundaryColumn);
		initFDWTColumn(&boundaryColumn, CHECKED_LOADS);
		initFDWT53Column(transformBuffer, colId, in, sizeX, sizeY, &boundaryColumn, firstY);
    }

    // index of column which will be written into output by this thread
    const int outColumnIndex = parityIdx(WIN_SIZE_X);

    // offset of column which will be written by this thread into output
    const int outColumnOffset = getColumnOffset(&transformBuffer->info, outColumnIndex);

    // initialize output writer for this thread
    const int outputFirstX = getGroupId(0) * WIN_SIZE_X + outColumnIndex;

    VerticalDWTBandWriter writer;
    initVerticalDWTBandWriter(&writer, CHECKED_WRITES, sizeX, sizeY, outputFirstX, firstY);

    // Sliding window iterations:
    // Each iteration assumes that first 3 pixels of each column are loaded.
    for(int w = 0; w < winSteps; w++) {
		// For each column (including boundary columns): load and vertically
		// transform another WIN_SIZE_Y lines.
		loadAndVerticallyTransform(transformBuffer, &column, in);
		if(getLocalId(0) < 3) {
			loadAndVerticallyTransform(transformBuffer, &boundaryColumn, in);
		}

	
		// wait for all columns to be vertically transformed and transform all
		// output rows horizontally
		localMemoryFence();
		forEachHorizontalOdd_Forward53Predict(transformBuffer, 2, WIN_SIZE_Y);
		localMemoryFence();
		forEachHorizontalEven_Forward53Update(transformBuffer, 2, WIN_SIZE_Y);

		
		// wait for all output rows to be transformed horizontally and write
		// them into output buffer
		localMemoryFence();
		for(int r = 2; r < (2 + WIN_SIZE_Y); r += 2) {
			// Write low coefficients from output column into low band ...
			writeLowIntoINT(&writer,out, transformBuffer->data[outColumnOffset + r * STRIDE]);
			// ... and high coeficients into the high band.
			writeHighIntoINT(&writer,out, transformBuffer->data[outColumnOffset + (r+1) * STRIDE]);
		}
	
		// before proceeding to next iteration, wait for all output columns
		// to be written into the output
		localMemoryFence();

	}
}


/// Main GPU 5/3 FDWT entry point.
/// @param in     input image ( reverse transformed image)
/// @param out    output buffer (5/3 transformed coefficients)
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