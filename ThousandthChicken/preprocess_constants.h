// License: please see LICENSE2 file for more details.
#pragma once


/* cuda block is a square with a side of BLOCK_SIZE. Actual number of threads in the block is the square of this value*/
#define BLOCK_SIZE 16
#define TILE_SIZEX 32
#define TILE_SIZEY 32

