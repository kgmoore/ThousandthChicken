// License: please see LICENSE4 file for more details.

#pragma once


// number of shared memory banks - needed for correct padding
#define SHM_BANKS 32

inline int divRndUp(const int n, const int d) {
	return n/d + !!(n % d); 
  }
