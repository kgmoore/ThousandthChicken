#pragma once

typedef struct _CodeBlockAdditionalInfo
{
	int length;
	unsigned char significantBits;
	unsigned char codingPasses;
	unsigned char width;
	unsigned char nominalWidth;
	unsigned char height;
	unsigned char nominalHeight;
	unsigned char stripeNo;
	unsigned char magbits;
	unsigned char subband;
	unsigned char compType;
	unsigned char dwtLevel;
	float stepSize;

	int magconOffset;

	int d_coefficientsOffset;
} CodeBlockAdditionalInfo;
