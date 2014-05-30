#pragma once

#include "dwt_common.cl"
#include "dwt_transform_buffer.cl"
#include "dwt_io.cl"


/// RDWTColumn //////////////////////////////////////////////////////////////////


/// Info needed for loading of one input column from input image.
/// @param CHECKED  true if loader should check boundaries
typedef struct  {

    bool CHECKED;
    /// loader of pixels from column in input image
    VerticalDWTBandLoader loader;
      
    /// Offset of corresponding column in shared buffer.
    int offset;

 
} RDWTColumn;


void initRDWTColumn(RDWTColumn* column, bool checked) {
   column->CHECKED = checked;
   column->offset = 0;
}


/// Sets all fields to some values to avoid 'uninitialized' warnings.
void clearRDWTColumn(RDWTColumn* column) {
	column->offset = 0;
	clearVerticalDWTBandLoader(&column->loader);
}


/////////////////////////////////////////////////////////////////////////////////////////


/// FDWTColumn //////////////////////////////////////////////////////////////////


/// Info needed for loading of one input column from input image.
/// @param CHECKED  true if loader should check boundaries
typedef struct  {

    bool CHECKED;
    /// loader of pixels from column in input image
    VerticalDWTPixelLoader loader;
      
    /// Offset of corresponding column in shared buffer.
    int offset;

	// only used for forward dwt 53 transform
	// backup of first 3 loaded pixels (not transformed)
    int pixel0, pixel1, pixel2;

 
} FDWTColumn;


void initFDWTColumn(FDWTColumn* column, bool checked) {
   column->CHECKED = checked;
   column->offset = 0;
}


/// Sets all fields to some values to avoid 'uninitialized' warnings.
void clearFDWTColumn(FDWTColumn* column) {
	column->offset = 0;
	column->pixel0 = column->pixel1 = column->pixel2 = 0;
	clearVerticalDWTPixelLoader(&column->loader);
}


/////////////////////////////////////////////////////////////////////////////////////////


