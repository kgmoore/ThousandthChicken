// License: please see LICENSE4 file for more details.
#pragma once

#include "platform.cl"
#include "dwt_common.h"

  //////////////////////////////////////////////////////////////////////////////////////////////
  /// Info relating to buffer (in local memory of GPU) where block of input image is stored.
  /// Odd and even columns are separated. (Generates less bank conflicts when using lifting scheme.)
  /// All even columns are stored first, then all odd columns.
  /// All operations expect SIZE_X threads.
  /// @param SIZE_X      width of the buffer excluding two boundaries 
  ///                    (Also equal to the number of threads participating on all operations)
  ///                     Must be divisible by 4.
  /// @param SIZE_Y      height of buffer (total number of lines)
  /// @param BOUNDARY_X  number of extra pixels at the left and right side
  ///                     boundary is expected to be smaller than half SIZE_X
  ///                     Must be divisible by 2, because half of the boundary lies on the left of the image,
  ///                     half lies on the right
typedef struct {
    int SIZE_X;
	int SIZE_Y;
	int BOUNDARY_X;

	/// difference between pointers to two vertical neigbours
	int VERTICAL_STRIDE;

	/// size of one of two buffers (odd or even)
    int BUFFER_SIZE;
      
    /// unused space between two buffers
    int PADDING;
      
      /// offset of the odd columns buffer from the beginning of data buffer
    int ODD_OFFSET;

	/// size of buffer for both even and odd columns
	int DATA_BUFFER_SIZE;

} TransformBufferInfo;

// constructor for transfer buffer info struct
void initTransformBufferInfo(LOCAL TransformBufferInfo* const info, int SIZE_X, int SIZE_Y, int BOUNDARY_X) {
   info->SIZE_X = SIZE_X;
   info->SIZE_Y = SIZE_Y;
   info->BOUNDARY_X = BOUNDARY_X;
   info->VERTICAL_STRIDE =  BOUNDARY_X + (SIZE_X / 2);
   info->BUFFER_SIZE = info->VERTICAL_STRIDE * SIZE_Y;
   info->PADDING = SHM_BANKS - ((info->BUFFER_SIZE + SHM_BANKS / 2) % SHM_BANKS);
   info->ODD_OFFSET = info->BUFFER_SIZE + info->PADDING;
   info->DATA_BUFFER_SIZE = 2 * info->BUFFER_SIZE + info->PADDING;

}


/// Wraps local memory buffer and methods for computing DWT using
/// lifting schema and sliding window.
/// @param info  transform buffer info struct
/// @param WIN_SIZE_X  width of the sliding window
/// @param WIN_SIZE_Y  height of the sliding window
typedef struct {
	TransformBufferInfo info;
	int WIN_SIZE_X;
	int WIN_SIZE_Y;
	LOCAL int* data;
} TransformBufferINT;

typedef struct {
	TransformBufferInfo info;
	int WIN_SIZE_X;
	int WIN_SIZE_Y;
	LOCAL float* data;
} TransformBufferFLOAT;

// constructor for transform buffer
void initTransformBufferINT(LOCAL TransformBufferINT* const transformBuffer, int winsizex, int winsizey, LOCAL int* data) {

    transformBuffer->WIN_SIZE_X = winsizex;
	transformBuffer->WIN_SIZE_Y = winsizey;
	initTransformBufferInfo(&transformBuffer->info,  winsizex, winsizey + 3, 2);
    transformBuffer->data = data;
}
void initTransformBufferFLOAT(LOCAL TransformBufferFLOAT* const transformBuffer, int winsizex, int winsizey, LOCAL float* data) {

    transformBuffer->WIN_SIZE_X = winsizex;
	transformBuffer->WIN_SIZE_Y = winsizey;
	initTransformBufferInfo(&transformBuffer->info,  winsizex, winsizey + 7, 4);
    transformBuffer->data = data;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////

// HORIZONTAL ROUTINES

//////////////////////////////////////////////////////////////////////////////////////////////////////  
/// Applies specified function to all central elements while also passing
/// previous and next elements as parameters.
/// @param count         count of central elements to apply function to
/// @param prevOffset    offset of first central element
/// @param midOffset     offset of first central element's predecessor
/// @param nextOffset    offset of first central element's successor
/// @param function      the function itself
void horizontalStep_Reverse53Update(LOCAL TransformBufferINT* buffer, 
                            const int count, const int prevOffset, 
                                const int midOffset, const int nextOffset) {

   LOCAL TransformBufferInfo* info = &buffer->info;
   LOCAL int* data = buffer->data;

    // number of unchecked iterations
    const int STEPS = count / info->SIZE_X;
      
    // items remaining after last unchecked iteration
    const int finalCount = count % info->SIZE_X; 
      
    // offset of items processed in last (checked) iteration
    const int finalOffset = count - finalCount;  

      
    // all threads perform fixed number of iterations ...
    for(int i = 0; i < STEPS; i++) {
		const int delta = i * info->SIZE_X + getLocalId(0);
		const int previous  = data[prevOffset + delta];
		const int next      = data[nextOffset + delta];
		data[midOffset + delta] -= (previous + next + 2) / 4;  // F.3, page 118, ITU-T Rec. T.800 final draft
    }
      
    // ... but not all threads participate on final iteration
    if(getLocalId(0) < finalCount) {
	    const int delta = finalOffset + getLocalId(0);
		const int previous    = data[prevOffset + delta];
		const int next        = data[nextOffset + delta];
		data[midOffset + delta] -= (previous + next + 2) / 4;  // F.3, page 118, ITU-T Rec. T.800 final draft
    }
}
void horizontalStep_Forward53Update(LOCAL TransformBufferINT* buffer, 
                            const int count, const int prevOffset, 
                                const int midOffset, const int nextOffset) {

   LOCAL TransformBufferInfo* info = &buffer->info;
   LOCAL int* data = buffer->data;

    // number of unchecked iterations
    const int STEPS = count / info->SIZE_X;
      
    // items remaining after last unchecked iteration
    const int finalCount = count % info->SIZE_X; 
      
    // offset of items processed in last (checked) iteration
    const int finalOffset = count - finalCount;  

      
    // all threads perform fixed number of iterations ...
    for(int i = 0; i < STEPS; i++) {
	    const int delta = i * info->SIZE_X + getLocalId(0);
		const int previous       = data[prevOffset + delta];
		const int next           = data[nextOffset +delta];
		data[midOffset + delta] += (previous + next + 2) / 4;  // F.3, page 118, ITU-T Rec. T.800 final draft
    }
      
    // ... but not all threads participate on final iteration
    if(getLocalId(0) < finalCount) {
	    const int delta         = finalOffset + getLocalId(0);
		const int previous      = data[prevOffset + delta];
		const int next          = data[nextOffset + delta];
		data[midOffset + delta] += (previous + next + 2) / 4;  // F.3, page 118, ITU-T Rec. T.800 final draft
    }
}
void horizontalStepScale(LOCAL TransformBufferFLOAT* buffer, 
                            const int count, const int prevOffset, 
                                const int midOffset, const int nextOffset,
								float scale) {

   LOCAL TransformBufferInfo* info = &buffer->info;
   LOCAL float* data = buffer->data;

    // number of unchecked iterations
    const int STEPS = count / info->SIZE_X;
      
    // items remaining after last unchecked iteration
    const int finalCount = count % info->SIZE_X; 
      
    // offset of items processed in last (checked) iteration
    const int finalOffset = count - finalCount;  

      
    // all threads perform fixed number of iterations ...
    for(int i = 0; i < STEPS; i++) {
	    const int delta = i * info->SIZE_X + getLocalId(0);
		const float previous       = data[prevOffset + delta];
		const float next           = data[nextOffset +delta];
		data[midOffset + delta] +=  scale * (previous + next);  
    }
      
    // ... but not all threads participate on final iteration
    if(getLocalId(0) < finalCount) {
	    const int delta         = finalOffset + getLocalId(0);
		const float previous      = data[prevOffset + delta];
		const float next          = data[nextOffset + delta];
		data[midOffset + delta] +=  scale * (previous + next);  
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////
void horizontalStep_Reverse53Predict(LOCAL TransformBufferINT* buffer, 
                            const int count, const int prevOffset, 
                                const int midOffset, const int nextOffset) {

   LOCAL TransformBufferInfo* info = &buffer->info;
   LOCAL int* data = buffer->data;

    // number of unchecked iterations
    const int STEPS = count / info->SIZE_X;
      
    // items remaining after last unchecked iteration
    const int finalCount = count % info->SIZE_X; 
      
    // offset of items processed in last (checked) iteration
    const int finalOffset = count - finalCount;  

      
    // all threads perform fixed number of iterations ...
    for(int i = 0; i < STEPS; i++) {
		const int delta       = i * info->SIZE_X + getLocalId(0);
		const int previous    = data[prevOffset + delta];
		const int next        = data[nextOffset + delta];
		data[midOffset + delta] += (previous + next) / 2;      // F.4, page 118, ITU-T Rec. T.800 final draft
    }
      
    // ... but not all threads participate on final iteration
    if(getLocalId(0) < finalCount) {
		const int delta         = finalOffset + getLocalId(0);
		const int previous      = data[prevOffset + delta];
		const int next          = data[nextOffset + delta];
		data[midOffset + delta] += (previous + next) / 2;      // F.4, page 118, ITU-T Rec. T.800 final draft
    }
}
void horizontalStep_Forward53Predict(LOCAL TransformBufferINT* buffer, 
                            const int count, const int prevOffset, 
                                const int midOffset, const int nextOffset) {

   LOCAL TransformBufferInfo* info = &buffer->info;
   LOCAL int* data = buffer->data;

    // number of unchecked iterations
    const int STEPS = count / info->SIZE_X;
      
    // items remaining after last unchecked iteration
    const int finalCount = count % info->SIZE_X; 
      
    // offset of items processed in last (checked) iteration
    const int finalOffset = count - finalCount;  

      
    // all threads perform fixed number of iterations ...
    for(int i = 0; i < STEPS; i++) {
	    const int delta = i * info->SIZE_X + getLocalId(0);
		const int previous    = data[prevOffset + delta];
		const int next        = data[nextOffset + delta];
		data[midOffset + delta] -= (previous + next) / 2;      // F.4, page 118, ITU-T Rec. T.800 final draft
    }
      
    // ... but not all threads participate on final iteration
    if(getLocalId(0) < finalCount) {
	    const int delta         = finalOffset + getLocalId(0);
		const int previous      = data[prevOffset + delta];
		const int next          = data[nextOffset + delta];
		data[midOffset + delta] -= (previous + next) / 2;      // F.4, page 118, ITU-T Rec. T.800 final draft
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Applies specified function to all horizontally even elements in 
/// specified lines. (Including even elements in boundaries except 
/// first even element in first left boundary.) SIZE_X threads participate 
/// and synchronization is needed before result can be used.
/// @param firstLine  index of first line
/// @param numLines   count of lines
/// @param func       function to be applied on all even elements
///                   parameters: previous (odd) element, the even
///                   element itself and finally next (odd) element
void forEachHorizontalEven_Reverse53Update(LOCAL TransformBufferINT* buffer,
	                                    const int firstLine,
                                        const int numLines) {

   LOCAL TransformBufferInfo* info = &buffer->info;

    // number of even elements to apply function to
    const int count = numLines * info->VERTICAL_STRIDE - 1;
    // offset of first even element
    const int centerOffset = firstLine * info->VERTICAL_STRIDE + 1;
    // offset of odd predecessor of first even element
    const int prevOffset = firstLine * info->VERTICAL_STRIDE + info->ODD_OFFSET;
    // offset of odd successor of first even element
    const int nextOffset = prevOffset + 1;
      
    // call generic horizontal step function
    horizontalStep_Reverse53Update(buffer, count, prevOffset, centerOffset, nextOffset);
}
void forEachHorizontalEven_Forward53Update(LOCAL TransformBufferINT* buffer,
	                                    const int firstLine,
                                        const int numLines) {

   LOCAL TransformBufferInfo* info = &buffer->info;

    // number of even elements to apply function to
    const int count = numLines * info->VERTICAL_STRIDE - 1;
    // offset of first even element
    const int centerOffset = firstLine * info->VERTICAL_STRIDE + 1;
    // offset of odd predecessor of first even element
    const int prevOffset = firstLine * info->VERTICAL_STRIDE + info->ODD_OFFSET;
    // offset of odd successor of first even element
    const int nextOffset = prevOffset + 1;
      
    // call generic horizontal step function
    horizontalStep_Forward53Update(buffer, count, prevOffset, centerOffset, nextOffset);
}
void forEachHorizontalEvenScale(LOCAL TransformBufferFLOAT* buffer,
	                                    const int firstLine,
                                        const int numLines,
										float scale) {

   LOCAL TransformBufferInfo* info = &buffer->info;

    // number of even elements to apply function to
    const int count = numLines * info->VERTICAL_STRIDE - 1;
    // offset of first even element
    const int centerOffset = firstLine * info->VERTICAL_STRIDE + 1;
    // offset of odd predecessor of first even element
    const int prevOffset = firstLine * info->VERTICAL_STRIDE + info->ODD_OFFSET;
    // offset of odd successor of first even element
    const int nextOffset = prevOffset + 1;
      
    // call generic horizontal step function
    horizontalStepScale(buffer, count, prevOffset, centerOffset, nextOffset, scale);
}    

/// Applies given function to all horizontally odd elements in specified
/// lines. (Including odd elements in boundaries except last odd element
/// in last right boundary.) SIZE_X threads participate and synchronization
/// is needed before result can be used.
/// @param firstLine  index of first line
/// @param numLines   count of lines
/// @param func       function to be applied on all odd elements
///                   parameters: previous (even) element, the odd
///                   element itself and finally next (even) element
void forEachHorizontalOdd_Reverse53Predict(LOCAL TransformBufferINT* buffer,
	                                    const int firstLine,
                                        const int numLines) {

   LOCAL TransformBufferInfo* info = &buffer->info;

    // number of odd elements to apply function to
    const int count = numLines * info->VERTICAL_STRIDE - 1;
    // offset of even predecessor of first odd element
    const int prevOffset = firstLine * info->VERTICAL_STRIDE;
    // offset of first odd element
    const int centerOffset = prevOffset + info->ODD_OFFSET;
    // offset of even successor of first odd element
    const int nextOffset = prevOffset + 1;
      
    // call generic horizontal step function
    horizontalStep_Reverse53Predict(buffer,count, prevOffset, centerOffset, nextOffset);
}
void forEachHorizontalOdd_Forward53Predict(LOCAL TransformBufferINT *buffer,
	                                    const int firstLine,
                                        const int numLines) {
										   
   LOCAL TransformBufferInfo* info = &buffer->info;
   
    // number of odd elements to apply function to
    const int count = numLines * info->VERTICAL_STRIDE - 1;
    // offset of even predecessor of first odd element
    const int prevOffset = firstLine * info->VERTICAL_STRIDE;
    // offset of first odd element
    const int centerOffset = prevOffset + info->ODD_OFFSET;
    // offset of even successor of first odd element
    const int nextOffset = prevOffset + 1;
      
    // call generic horizontal step function
    horizontalStep_Forward53Predict(buffer,count, prevOffset, centerOffset, nextOffset);
}
void forEachHorizontalOddScale(LOCAL TransformBufferFLOAT *buffer,
	                                    const int firstLine,
                                        const int numLines,
										float scale) {
										   
   LOCAL TransformBufferInfo* info = &buffer->info;
   
    // number of odd elements to apply function to
    const int count = numLines * info->VERTICAL_STRIDE - 1;
    // offset of even predecessor of first odd element
    const int prevOffset = firstLine * info->VERTICAL_STRIDE;
    // offset of first odd element
    const int centerOffset = prevOffset + info->ODD_OFFSET;
    // offset of even successor of first odd element
    const int nextOffset = prevOffset + 1;
      
    // call generic horizontal step function
    horizontalStepScale(buffer,count, prevOffset, centerOffset, nextOffset,scale);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

// VERTICAL ROUTINES

///////////////////////////////////////////////////////////////////////////////////////////////////////////// 
/// Applies specified function to all even elements (except element #0)
/// of given column. Each thread takes care of one column, so there's 
/// no need for synchronization.
/// @param columnOffset  offset of thread's column
/// @param f             function to be applied on all even elements
///                      parameters: previous (odd) element, the even
///                      element itself and finally next (odd) element
void forEachVerticalEven_Reverse53Predict(LOCAL TransformBufferINT* buffer, const int columnOffset) {
   LOCAL TransformBufferInfo* info = &buffer->info;
   LOCAL int* data = buffer->data;
	const int steps = info->SIZE_Y / 2 - 1;
	for(int i = 0; i < steps; i++) {
		const int row = 2 + i * 2;
		const int prev = data[columnOffset + (row - 1) * info->VERTICAL_STRIDE];
		const int next = data[columnOffset + (row + 1) * info->VERTICAL_STRIDE];
		data[columnOffset + row * info->VERTICAL_STRIDE]  += (prev + next) / 2;      // F.4, page 118, ITU-T Rec. T.800 final draft
	}
}  
void forEachVerticalEven_Forward53Update(LOCAL TransformBufferINT *buffer, const int columnOffset) {
   LOCAL TransformBufferInfo* info = &buffer->info;
   LOCAL int* data = buffer->data;
	const int steps = info->SIZE_Y / 2 - 1;
	for(int i = 0; i < steps; i++) {
		const int row = 2 + i * 2;
		const int prev = data[columnOffset + (row - 1) * info->VERTICAL_STRIDE];
		const int next = data[columnOffset + (row + 1) * info->VERTICAL_STRIDE];
		data[columnOffset + row * info->VERTICAL_STRIDE] += (prev + next + 2) / 4; // F.9, page 126, ITU-T Rec. T.800 final draft
	}
}
void forEachVerticalEvenScale(LOCAL TransformBufferFLOAT *buffer, const int columnOffset, float scale) {
   LOCAL TransformBufferInfo* info = &buffer->info;
   LOCAL float* data = buffer->data;
	const int steps = info->SIZE_Y / 2 - 1;
	for(int i = 0; i < steps; i++) {
		const int row = 2 + i * 2;
		const int prev = data[columnOffset + (row - 1) * info->VERTICAL_STRIDE];
		const int next = data[columnOffset + (row + 1) * info->VERTICAL_STRIDE];
		data[columnOffset + row * info->VERTICAL_STRIDE] += (prev + next) * scale; 
	}
}
  
/// Applies specified function to all odd elements of given column.
/// Each thread takes care of one column, so there's no need for
/// synchronization.
/// @param columnOffset  offset of thread's column
/// @param f             function to be applied on all odd elements
///                      parameters: previous (even) element, the odd
///                      element itself and finally next (even) element
void forEachVerticalOdd_Reverse53Update(LOCAL TransformBufferINT* buffer, const int columnOffset) {
   LOCAL TransformBufferInfo* info = &buffer->info;
   LOCAL int* data = buffer->data;
    const int steps = (info->SIZE_Y - 1) / 2;
    for(int i = 0; i < steps; i++) {
		const int row = i * 2 + 1;
		const int prev = data[columnOffset + (row - 1) * info->VERTICAL_STRIDE];
		const int next = data[columnOffset + (row + 1) * info->VERTICAL_STRIDE];
		data[columnOffset + row * info->VERTICAL_STRIDE] -= (prev + next + 2) / 4;  // F.3, page 118, ITU-T Rec. T.800 final draft
    }
}
void forEachVerticalOdd_Forward53Predict(LOCAL TransformBufferINT* buffer, const int columnOffset) {
   LOCAL TransformBufferInfo* info = &buffer->info;
   LOCAL int* data = buffer->data;
    const int steps = (info->SIZE_Y - 1) / 2;
    for(int i = 0; i < steps; i++) {
		const int row = i * 2 + 1;
		const int prev = data[columnOffset + (row - 1) * info->VERTICAL_STRIDE];
		const int next = data[columnOffset + (row + 1) * info->VERTICAL_STRIDE];
		data[columnOffset + row * info->VERTICAL_STRIDE] -= (prev + next ) / 2;  // F.8, page 126, ITU-T Rec. T.800 final draft
    }
}
void forEachVerticalOddScale(LOCAL TransformBufferFLOAT* buffer, const int columnOffset, float scale) {
   LOCAL TransformBufferInfo* info = &buffer->info;
   LOCAL float* data = buffer->data;
    const int steps = (info->SIZE_Y - 1) / 2;
    for(int i = 0; i < steps; i++) {
		const int row = i * 2 + 1;
		const int prev = data[columnOffset + (row - 1) * info->VERTICAL_STRIDE];
		const int next = data[columnOffset + (row + 1) * info->VERTICAL_STRIDE];
		data[columnOffset + row * info->VERTICAL_STRIDE] += (prev + next ) * scale;  
    }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



/// Gets transform buffer offset of the column with given index. 
//  Central columns have indices from 0 to SIZE_X - 1, 
//  left boundary columns have negative indices,
/// and right boundary columns indices start with SIZE_X.
/// @param columnIndex  index of column in input buffer
/// @return  offset (in transform buffer) of the first item of the column with specified index
int getColumnOffset(LOCAL TransformBufferInfo* info, int columnIndex) {
    columnIndex += info->BOUNDARY_X;             // skip boundary
    return columnIndex / 2                 // select right column
        + (columnIndex & 1) * info->ODD_OFFSET;  // select odd or even buffer
}


/// Scales elements at specified lines.
/// @param evenScale  scaling factor for horizontally even elements
/// @param oddScale   scaling factor for horizontally odd elements
/// @param numLines   number of lines, whose elements should be scaled
/// @param firstLine  index of first line to scale elements in
void scaleHorizontal(LOCAL TransformBufferFLOAT* transformBuffer, const int evenScale, const int oddScale,
                                const int firstLine, const int numLines) {
								
	LOCAL TransformBufferInfo* info = &transformBuffer->info;
	LOCAL float* data = transformBuffer->data;
    const int offset = firstLine * info->VERTICAL_STRIDE;
    const int count = numLines * info->VERTICAL_STRIDE;
    const int steps = count / info->SIZE_X;
    const int finalCount = count % info->SIZE_X;
    const int finalOffset = count - finalCount;
      
    // run iterations, whete all threads participate
    for(int i = 0; i < steps; i++) {
		data[getLocalId(0) + i * info->SIZE_X + offset] *= evenScale;
		data[getLocalId(0) + i * info->SIZE_X + offset + info->ODD_OFFSET] *= oddScale;
    }
      
    // some threads also finish remaining unscaled items
    if(getLocalId(0) < finalCount) {
		data[getLocalId(0) + finalOffset + offset] *= evenScale;
		data[getLocalId(0) + finalOffset + offset + info->ODD_OFFSET] *= oddScale;
    }
}


/// Scales elements in specified column.
/// @param evenScale     scaling factor for vertically even elements
/// @param oddScale      scaling factor for vertically odd elements
/// @param columnOffset  offset of the column to work with
/// @param numLines      number of lines, whose elements should be scaled
/// @param firstLine     index of first line to scale elements in
void scaleVertical(LOCAL TransformBufferFLOAT* transformBuffer,
	                            const int evenScale, const int oddScale,
                                const int columnOffset, const int numLines,
                                const int firstLine) {
	LOCAL TransformBufferInfo* info = &transformBuffer->info;
	LOCAL float* data = transformBuffer->data;
    for(int i = firstLine; i < (numLines + firstLine); i++) {
		if(i & 1) {
			data[columnOffset + i * info->VERTICAL_STRIDE] *= oddScale;
		} else {
			data[columnOffset + i * info->VERTICAL_STRIDE] *= evenScale;
		}
    }
}

