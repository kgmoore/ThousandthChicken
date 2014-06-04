#pragma once

#include "platform.cl"

CONSTANT float Wr = 0.299f;
CONSTANT float Wb = 0.114f;
//CONSTANT float Wg = 1.0f - Wr - Wb;
CONSTANT float Wg = 1.0f - 0.299f - 0.114f;
CONSTANT float Umax = 0.436f;
CONSTANT float Vmax = 0.615f;

