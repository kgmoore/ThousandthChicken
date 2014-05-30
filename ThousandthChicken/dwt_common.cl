#pragma once

#include "platform.cl"
#include "dwt_common.h"

	  
// FDWT 9/7 scaling coefficients
CONSTANT float scale97Mul = 1.23017410491400f;
CONSTANT float scale97Div = 1.0 / 1.23017410491400f;
  
 
/// Returns index ranging from 0 to num threads, such that first half
/// of threads get even indices and others get odd indices. Each thread
/// gets different index.
/// Example: (for 8 threads)   threadIdx.x:   0  1  2  3  4  5  6  7
///                              parityIdx:   0  2  4  6  1  3  5  7
/// @param THREADS  total count of participating threads
/// @return parity-separated index of thread
inline int parityIdx(int THREADS) {
	return (getLocalId(0) * 2) - (THREADS - 1) * (getLocalId(0) / (THREADS / 2));
}

