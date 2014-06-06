// License: please see LICENSE4 file for more details.
#include "dwt_common.cl"

/// Handles mirroring of image at edges in a DWT correct way.
/// @param d      a position in the image (will be replaced by mirrored d)
/// @param sizeD  size of the image along the dimension of 'd'
static void mirror(int *d, const int sizeD) {
    // TODO: enable multiple mirroring:
//      if(sizeD > 1) {
//        if(d < 0) {
//          const int underflow = -1 - d;
//          const int phase = (underflow / (sizeD - 1)) & 1;
//          const int remainder = underflow % (sizeD - 1);
//          if(phase == 0) {
//            d = remainder + 1;
//          } else {
//            d = sizeD - 2 - remainder;
//          }
//        } else if(d >= sizeD) {
//          const int overflow = d - sizeD;
//          const int phase = (overflow / (sizeD - 1)) & 1;
//          const int remainder = overflow % (sizeD - 1);
//          if(phase == 0) {
//            d = sizeD - 2 - remainder;
//          } else {
//            d = remainder + 1;
//          }
//        }
//      } else {
//        d = 0;
//      }
    int dval = *d;
    if(dval >= sizeD) {
		*d = 2 * sizeD - 2 - dval;
    } else if(dval < 0) {
		*d = -dval;
    }
}

///// VerticalDWTPixelIO  ////////////////////////////////////////////


/// Base class for pixel loader and writer - manages computing start index,
/// stride and end of image for loading column of pixels.
/// @param CHECKED  true = be prepared to check image boundary, false = don't care
typedef struct {
	bool CHECKED;
    int end;         ///< index of bottom neightbor of last pixel of column
    int stride; 
	 
} VerticalDWTPixelIO;

int initVerticalDWTPixelIO(VerticalDWTPixelIO* pixIo, bool checked, 
                              const int sizeX, const int sizeY,
                              int firstX, int firstY) {
	 pixIo->CHECKED = checked;
     // initialize all pointers and stride
      pixIo->end = sizeY * sizeX;
      pixIo->stride = sizeX;
      return firstX + sizeX * firstY;
}

////// VerticalDWTPixelWriter //////////////////////////////////////////////

/// Writes reverse transformed pixels directly into output image.
/// @param CHECKED  true = be prepared to check for image boundary, false = don't care
typedef struct {
    VerticalDWTPixelIO pixIo;
	int next;   // index of the next pixel to be loaded

}  VerticalDWTPixelWriter;


/// Initializes writer - sets output buffer and a position of first pixel.
/// @param sizeX   width of the image
/// @param sizeY   height of the image
/// @param firstX  x-coordinate of first pixel to write into
/// @param firstY  y-coordinate of first pixel to write into
void initVerticalDWTPixelWriter(VerticalDWTPixelWriter* pixWriter, bool checked, 
                              const int sizeX, const int sizeY, 
                              int firstX, int firstY) {
	VerticalDWTPixelIO* pixIo = &pixWriter->pixIo;
    if(firstX < sizeX) {
		 pixWriter->next = initVerticalDWTPixelIO(pixIo, checked, sizeX, sizeY, firstX, firstY);
    } else {
		pixIo->end = 0;
		pixIo->stride = 0;
		pixWriter->next = 0;
    }
}

/// Writes given value at next position and advances internal pointer while
/// correctly handling mirroring.
/// @param output  output image to write pixel into
/// @param value   value of the pixel to be written
void writeIntoINT(VerticalDWTPixelWriter* pixWriter, 
                     GLOBAL int * const output, const int value) {
	VerticalDWTPixelIO* pixIo = &pixWriter->pixIo;
    if((!pixIo->CHECKED) || (pixWriter->next < pixIo->end)) {
		output[pixWriter->next] = value;
		pixWriter->next += pixIo->stride;
    }
}
void writeIntoFLOAT(VerticalDWTPixelWriter* pixWriter, 
                     GLOBAL float * const output, const float value) {
	VerticalDWTPixelIO* pixIo = &pixWriter->pixIo;
    if((!pixIo->CHECKED) || (pixWriter->next < pixIo->end)) {
		output[pixWriter->next] = value;
		pixWriter->next += pixIo->stride;
    }
}

//////  VerticalDWTPixelLoader ////////////////////////////


/// Loads pixels from input image.
/// @param T        type of image input pixels
/// @param CHECKED  true = be prepared to image boundary, false = don't care
typedef struct {
    VerticalDWTPixelIO pixIo;
    int last;  ///< index of last loaded pixel

}  VerticalDWTPixelLoader;

/// Initializes loader - sets input size and a position of first pixel.
/// @param sizeX   width of the image
/// @param sizeY   height of the image
/// @param firstX  x-coordinate of first pixel to load
/// @param firstY  y-coordinate of first pixel to load
void initVerticalDWTPixelLoader(VerticalDWTPixelLoader* pixLoader, bool checked,
	                    const int sizeX, const int sizeY,
                        int firstX, int firstY) {
    // correctly mirror x coordinate
    mirror(&firstX, sizeX);
      
    // 'last' always points to already loaded pixel (subtract sizeX = stride)
    pixLoader->last = initVerticalDWTPixelIO(&pixLoader->pixIo, checked, sizeX, sizeY, firstX, firstY) - sizeX;
}
    
/// Sets all fields to zeros, for compiler not to complain about
/// uninitialized stuff.
void clearVerticalDWTPixelLoader(VerticalDWTPixelLoader* pixLoader) {
	VerticalDWTPixelIO* pixIo = &pixLoader->pixIo;
    pixIo->end = 0;
    pixIo->stride = 0;
    pixLoader->last = 0;
}

/// Gets another pixel and advancees internal pointer to following one.
/// @param input  input image to load next pixel from
/// @return next pixel from given image
int loadFromINT(VerticalDWTPixelLoader* pixLoader,
                GLOBAL const int * const input) {
	VerticalDWTPixelIO* pixIo = &pixLoader->pixIo;
    pixLoader->last += pixIo->stride;

	//mirror in vertical direction
    if(pixIo->CHECKED && (pixLoader->last >= pixIo->end)) {
		pixIo->stride *= -1; // reverse loader's direction
		pixLoader->last += 2 * pixIo->stride;
    }
    return input[pixLoader->last];
}
float loadFromFLOAT(VerticalDWTPixelLoader* pixLoader,
                GLOBAL const float * const input) {
	VerticalDWTPixelIO* pixIo = &pixLoader->pixIo;
    pixLoader->last += pixIo->stride;

	//mirror in vertical direction
    if(pixIo->CHECKED && (pixLoader->last >= pixIo->end)) {
		pixIo->stride *= -1; // reverse loader's direction
		pixLoader->last += 2 * pixIo->stride;

    }
    return input[pixLoader->last];
}

/// VerticalDWTBandIO /////////////////////////////////////////


/// Base for band write and loader. Manages computing strides and pointers
/// to first and last pixels in a linearly-stored-bands correct way.
/// @param CHECKED  true = be prepared to check image boundary, false = don't care
typedef struct {
 
    bool CHECKED;

    /// index of bottom neighbor of last pixel of loaded column
    int end;
    
    /// increment of index to get from highpass band to the lowpass one
    int strideHighToLow;
    
    /// increment of index to get from the lowpass band to the highpass one
    int strideLowToHigh;

} VerticalDWTBandIO;

/// Initializes IO - sets size of image and a position of first pixel.
/// @param imageSizeX   width of the image
/// @param imageSizeY   height of the image
/// @param firstX       x-coordinate of first pixel to use
///                     (Parity determines vertically low or high band.)
/// @param firstY       y-coordinate of first pixel to use
///                     (Parity determines horizontally low or high band.)
/// @return index of first item specified by firstX and firstY
 int initVerticalDWTBandIO(VerticalDWTBandIO* bandIo, bool checked,
                             const int imageSizeX, const int imageSizeY,
                            int firstX, int firstY) {
	bandIo->CHECKED = checked;

    // index of first pixel (topmost one) of the column with index firstX
    int columnOffset = firstX / 2;
      
    // difference between indices of two vertically neighboring pixels
    // in the same band
    int verticalStride = imageSizeX / 2;
      
    // resolve index of first pixel according to horizontal parity
    if(firstX & 1) {
		// first pixel in one of right bands
		columnOffset += divRndUp(imageSizeX, 2) * divRndUp(imageSizeY, 2);
		bandIo->strideLowToHigh = (imageSizeX * imageSizeY) / 2;
    } else {
		// first pixel in one of left bands
		verticalStride += imageSizeX & 1;
		bandIo->strideLowToHigh = divRndUp(imageSizeY, 2)  * imageSizeX;
    }
      
    // set the other stride
    bandIo->strideHighToLow = verticalStride - bandIo->strideLowToHigh;

    // compute index of coefficient which indicates end of image
    if(bandIo->CHECKED) {
		bandIo->end = columnOffset                    // right column
				+ (imageSizeY / 2) * verticalStride   // right row
				+ /*(imageSizeY & 1) * */bandIo->strideLowToHigh; // possibly in high band
    } else {
	   bandIo->end = 0;
    }
    // finally, return index of the first item
    return columnOffset                        // right column
            + (firstY / 2) * verticalStride    // right row
            + (firstY & 1) * bandIo->strideLowToHigh;  // possibly in high band
}

////////  VerticalDWTBandLoader  ///////////////////////////////////////


/// Directly loads coefficients from four consecutively stored transformed
/// bands.
/// @param T        type of input band coefficients
/// @param CHECKED  true = be prepared to image boundary, false = don't care
typedef struct {
   VerticalDWTBandIO bandIo;
   int last;  ///< index of last loaded pixel

} VerticalDWTBandLoader ;


/// Initializes loader - sets input size and a position of first pixel.
/// @param imageSizeX   width of the image
/// @param imageSizeY   height of the image
/// @param firstX       x-coordinate of first pixel to load
///                     (Parity determines vertically low or high band.)
/// @param firstY       y-coordinate of first pixel to load
///                     (Parity determines horizontally low or high band.)
void initVerticalDWTBandLoader(VerticalDWTBandLoader* bandLoader, bool CHECKED,
                        const int imageSizeX, const int imageSizeY,
                        int firstX, const int firstY) {
	VerticalDWTBandIO* bandIo = &bandLoader->bandIo;
    mirror(&firstX, imageSizeX);
    bandLoader->last = initVerticalDWTBandIO(bandIo, CHECKED, imageSizeX, imageSizeY, firstX, firstY);
      
    // adjust to point to previous item
    bandLoader->last -= (firstY & 1) ? bandIo->strideLowToHigh : bandIo->strideHighToLow; 
}
    
/// Sets all fields to zeros, for compiler not to complain about
/// uninitialized stuff.
void clearVerticalDWTBandLoader(VerticalDWTBandLoader* bandLoader) {
	VerticalDWTBandIO* bandIo = &bandLoader->bandIo;
    bandIo->end = 0;
    bandIo->strideHighToLow = 0;
    bandIo->strideLowToHigh = 0;
    bandLoader->last = 0;
}

/// Checks internal index and possibly reverses direction of loader.
/// (Handles mirroring at the bottom of the image.)
/// @param input   input image to load next coefficient from
/// @param stride  stride to use now (one of two loader's strides)
/// @return loaded coefficient
int updateAndLoadINT(VerticalDWTBandLoader* bandLoader,
                    GLOBAL const int * const input, const int  stride) {
	VerticalDWTBandIO* bandIo = &bandLoader->bandIo;
    bandLoader->last += stride;
    if(bandIo->CHECKED && (bandLoader->last >= bandIo->end)) {
		// undo last two updates of index (to get to previous mirrored item)
		bandLoader->last -= (bandIo->strideLowToHigh + bandIo->strideHighToLow);

		// swap and reverse strides (to move up in the loaded column now)
		const int temp = bandIo->strideLowToHigh;
		bandIo->strideLowToHigh = -bandIo->strideHighToLow;
		bandIo->strideHighToLow = -temp;
    }
    // avoid reading from negative indices if loader is checked
    // return (CHECKED && (last < 0)) ? 0 : input[last];  // TODO: use this checked variant later
    return input[bandLoader->last];
}
float updateAndLoadFLOAT(VerticalDWTBandLoader* bandLoader,
                    GLOBAL const float * const input, const int  stride) {
	VerticalDWTBandIO* bandIo = &bandLoader->bandIo;
    bandLoader->last += stride;
    if(bandIo->CHECKED && (bandLoader->last >= bandIo->end)) {
		// undo last two updates of index (to get to previous mirrored item)
		bandLoader->last -= (bandIo->strideLowToHigh + bandIo->strideHighToLow);

		// swap and reverse strides (to move up in the loaded column now)
		const int temp = bandIo->strideLowToHigh;
		bandIo->strideLowToHigh = -bandIo->strideHighToLow;
		bandIo->strideHighToLow = -temp;
    }
    // avoid reading from negative indices if loader is checked
    // return (CHECKED && (last < 0)) ? 0 : input[last];  // TODO: use this checked variant later
    return input[bandLoader->last];
}

/// Gets another coefficient from lowpass band and advances internal index.
/// Call this method first if position of first pixel passed to init
/// was in high band.
/// @param input   input image to load next coefficient from
/// @return next coefficient from the lowpass band of the given image
int loadLowFromINT(VerticalDWTBandLoader* bandLoader, GLOBAL const int * const input) {
    return updateAndLoadINT(bandLoader,input, bandLoader->bandIo.strideHighToLow);
}
float loadLowFromFLOAT(VerticalDWTBandLoader* bandLoader, GLOBAL const float * const input) {
    return updateAndLoadFLOAT(bandLoader,input, bandLoader->bandIo.strideHighToLow);
}

/// Gets another coefficient from the highpass band and advances index.
/// Call this method first if position of first pixel passed to init
/// was in high band.
/// @param input   input image to load next coefficient from
/// @return next coefficient from the highbass band of the given image
int loadHighFromINT(VerticalDWTBandLoader* bandLoader, GLOBAL const int * const input) {
    return updateAndLoadINT(bandLoader,input, bandLoader->bandIo.strideLowToHigh);
}
float loadHighFromFLOAT(VerticalDWTBandLoader* bandLoader, GLOBAL const float * const input) {
    return updateAndLoadFLOAT(bandLoader,input, bandLoader->bandIo.strideLowToHigh);
}
/////////////////////////////  VerticalDWTBandWriter /////////////////////////////////////////////////


/// Directly saves coefficients into four transformed bands.
/// @param CHECKED  true = be prepared to image boundary, false = don't care
typedef struct {
   VerticalDWTBandIO bandIo;
   int next;  ///< index of last loaded pixel

} VerticalDWTBandWriter ;


/// Checks internal index and possibly stops the writer.
/// (Handles mirroring at edges of the image.)
/// @param output  output buffer
/// @param item    item to put into the output
/// @param stride  increment of the pointer to get to next output index
void saveAndUpdateINT(VerticalDWTBandWriter* bandWriter, GLOBAL int * const output, const int  item, const int stride) {
    //AB - changing bounds checking algorithm
	//if( (!bandWriter->bandIo.CHECKED) || (bandWriter->next != bandWriter->bandIo.end)) {
	if( (!bandWriter->bandIo.CHECKED) || (bandWriter->next < bandWriter->bandIo.end) ) {
		output[bandWriter->next] = item;
		bandWriter->next += stride;
    }
}
void saveAndUpdateFLOAT(VerticalDWTBandWriter* bandWriter, GLOBAL float * const output, const float  item, const int stride) {
    //AB - changing bounds checking algorithm
	//if( (!bandWriter->bandIo.CHECKED) || (bandWriter->next != bandWriter->bandIo.end)) {
	if( (!bandWriter->bandIo.CHECKED) || (bandWriter->next < bandWriter->bandIo.end) ) {
		output[bandWriter->next] = item;
		bandWriter->next += stride;
    }
}


/// Sets all fields to zeros, for compiler not to complain about
/// uninitialized stuff.
void clearVerticalDWTBandWriter(VerticalDWTBandWriter* bandWriter) {
	bandWriter->bandIo.end = 0;
	bandWriter->bandIo.strideHighToLow = 0;
	bandWriter->bandIo.strideLowToHigh = 0;
	bandWriter->next = 0;
}

/// Initializes writer - sets output size and a position of first pixel.
/// @param output       output image
/// @param imageSizeX   width of the image
/// @param imageSizeY   height of the image
/// @param firstX       x-coordinate of first pixel to write
///                     (Parity determines vertically low or high band.)
/// @param firstY       y-coordinate of first pixel to write
///                     (Parity determines horizontally low or high band.)
void initVerticalDWTBandWriter(VerticalDWTBandWriter* bandWriter, bool CHECKED, const int imageSizeX, const int imageSizeY, const int firstX, const int firstY) {
	if (firstX < imageSizeX) {
		bandWriter->next = initVerticalDWTBandIO(&bandWriter->bandIo, CHECKED, imageSizeX, imageSizeY, firstX, firstY);
	} else {
		clearVerticalDWTBandWriter(bandWriter);
	}
}
   

/// Writes another coefficient into the band which was specified using
/// init's firstX and firstY parameters and advances internal pointer.
/// Call this method first if position of first pixel passed to init
/// was in lowpass band.
/// @param output  output image
/// @param low     lowpass coefficient to save into the lowpass band
void writeLowIntoINT(VerticalDWTBandWriter* bandWriter, GLOBAL int * const output, const int  primary) {
    saveAndUpdateINT(bandWriter,output, primary, bandWriter->bandIo.strideLowToHigh);
}
void writeLowIntoFLOAT(VerticalDWTBandWriter* bandWriter, GLOBAL float * const output, const float  primary) {
    saveAndUpdateFLOAT(bandWriter,output, primary, bandWriter->bandIo.strideLowToHigh);
}

/// Writes another coefficient from the other band and advances pointer.
/// Call this method first if position of first pixel passed to init
/// was in highpass band.
/// @param output  output image
/// @param high    highpass coefficient to save into the highpass band
void writeHighIntoINT(VerticalDWTBandWriter* bandWriter, GLOBAL int * const output, const int other) {
    saveAndUpdateINT(bandWriter,output, other, bandWriter->bandIo.strideHighToLow);
}
void writeHighIntoFLOAT(VerticalDWTBandWriter* bandWriter, GLOBAL float * const output, const float other) {
    saveAndUpdateFLOAT(bandWriter,output, other, bandWriter->bandIo.strideHighToLow);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////