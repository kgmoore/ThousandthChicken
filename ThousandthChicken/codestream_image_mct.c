// License: please see LICENSE2 file for more details.
#include <stdlib.h>

#include "codestream_image_mct.h"
#include "io_buffered_stream.h"
#include "codestream_image.h"
#include "logger.h"
#include "codestream_markers.h"


/**
 * @brief Reads MCT marker.
 *
 * @param buffer
 * @param img
*/ 
void read_mct_marker(type_buffer *buffer, type_image *img) {
	int marker;
	int length;
	unsigned short Smct;
	unsigned char type;
	int i;
	type_mct *old_mcts, *mct;

	/* Read MCT Marker */
	marker = read_buffer(buffer, 2);
	if(marker != MCT)
	{
		println_var(INFO, "Error: Expected MCT marker instead of %x", marker);
	}

	length = read_buffer(buffer, 4)-5;

	/* Read Smct */
	Smct = read_byte(buffer);
	
	type = (Smct&(3<<4))>>4;

	old_mcts = img->mct_data->mcts[type];
	img->mct_data->mcts[type] = (type_mct*)realloc(img->mct_data->mcts[type], sizeof(type_mct) * (++img->mct_data->mcts_count[type]));
	mct = &img->mct_data->mcts[type][img->mct_data->mcts_count[type]-1];
	
	if(img->mct_data->mcts[type] == NULL) {
		img->mct_data->mcts[type] = old_mcts;
		--img->mct_data->mcts_count[type];
		println_var(INFO, "Error: Memory reallocation failed! Ignoring MCT with Smct %u Skipping data", Smct);
		for(i=0; i<length-1; ++i) {
			read_byte(buffer);
		}
	} else {
		mct->index = Smct&0x0F;
		mct->type = type;
		mct->element_type = (Smct&(3<<6))>>6;
		mct->length = length/(1<<mct->element_type);
		mct->data = (unsigned char*)malloc(length);
		for(i=0; i<length; ++i) {
			mct->data[i] = read_byte(buffer);
		}
	}
}


/**
 * @brief Reads MMC marker.
 *
 * @param buffer
 * @param img
*/ 
void read_mcc_marker(type_buffer *buffer, type_image *img) {
	int marker;
	int length;
	unsigned short i;
	int count=0;
	unsigned short temp_16;
	unsigned char temp_8;
	type_mcc *old_mccs, *mcc;
	type_mcc_data* mcc_data = NULL;

	old_mccs = img->mct_data->mccs;
	img->mct_data->mccs = (type_mcc*)realloc(img->mct_data->mccs, sizeof(type_mcc) * (++img->mct_data->mccs_count));
	mcc = &img->mct_data->mccs[img->mct_data->mccs_count-1];
	mcc->data = NULL;



	if(img->mct_data->mccs == NULL) {
		img->mct_data->mccs = old_mccs;
		--img->mct_data->mccs_count;
		println_var(INFO, "Error: Memory reallocation failed! Aborting read of MCC segment");
		return;
	}

	/* Read MCC Marker */
	marker = read_buffer(buffer, 2);
	if(marker != MCC)
	{
		println_var(INFO, "Error: Expected MCC marker instead of %x", marker);
	}

	length = read_buffer(buffer, 4)-5;
	mcc->index = read_byte(buffer);
	/* reading unknown number of component collections */ 
	while(length>0) {
		mcc->data = (type_mcc_data*)realloc((void*)mcc->data, sizeof(type_mcc_data)*(++count));
		mcc_data = &mcc->data[count-1];

		/* input component collection header */
		mcc_data->type = read_byte(buffer)&3;
		temp_16 = read_buffer(buffer,2);
		mcc_data->input_count = temp_16 & 0x1FFF;
		mcc_data->input_component_type = temp_16 >> 14;
		length-=3;

		/* input component collection data */
		temp_16 = mcc_data->input_count * (1<<mcc_data->input_component_type);
		length-=temp_16;
		mcc_data->input_components = (unsigned char*)calloc(temp_16, sizeof(unsigned char));
		for(i=0; i<temp_16; ++i) {
			mcc_data->input_components[i] = read_byte(buffer);
		}

		/* output component collection header */
		temp_16 = read_buffer(buffer,2);
		mcc_data->output_count = temp_16 & 0x1FFF;
		mcc_data->output_component_type = temp_16 >> 14;
		length-=2;

		/* output component collection data */
		temp_16 = mcc_data->output_count * (1<<mcc_data->output_component_type);
		length-=temp_16;
		mcc_data->output_components = (unsigned char*)calloc(temp_16, sizeof(unsigned char));
		for(i=0; i<temp_16; ++i) {
			mcc_data->output_components[i] = read_byte(buffer);
		}

		/* component collection footer */
		temp_8 = read_byte(buffer);
		if(mcc_data->type & 2) {
			/* wavelet based decorrelation */
			mcc_data->atk = temp_8 & 0xF;
			mcc_data->ads = temp_8 >> 4;
			mcc_data->decorrelation_transform_matrix = 0x0;
			mcc_data->deccorelation_transform_offset = 0x0;
		} else {
			/* matrix based decorrelation */
			mcc_data->atk = 0x0;
			mcc_data->ads = 0x0;
			mcc_data->decorrelation_transform_matrix = temp_8 & 0xF;
			mcc_data->deccorelation_transform_offset = temp_8 >> 4;
		}
		length-=1;

	}
	mcc->count = count;
}

/**
 * @brief Reads MIC marker.
 *
 * @param buffer
 * @param img
*/ 
void read_mic_marker(type_buffer *buffer, type_image *img) {
	int marker;
	int length;
	unsigned short i;
	int count=0;
	unsigned short temp_16;
	unsigned char temp_8;
	type_mic *old_mics, *mic;
	type_mic_data* mic_data;

	old_mics = img->mct_data->mics;
	img->mct_data->mics = (type_mic*)realloc(img->mct_data->mics, sizeof(type_mic) * (++img->mct_data->mics_count));
	mic = &img->mct_data->mics[img->mct_data->mics_count-1];

	if(img->mct_data->mics == NULL) {
		img->mct_data->mics = old_mics;
		--img->mct_data->mics_count;
		println_var(INFO, "Error: Memory reallocation failed! Aborting read of MIC segment");
		return;
	}



	/* Read MCC Marker */
	marker = read_buffer(buffer, 2);
	if(marker != MIC)
	{
		println_var(INFO, "Error: Expected MIC marker instead of %x", marker);
	}

	length = read_buffer(buffer, 4)-5;
	mic->index = read_byte(buffer);
	/* reading unknown number of component collections */ 
	while(length>0) {
		mic->data = (type_mic_data*)realloc((void*)mic->data, sizeof(type_mic_data)*++count);
		mic_data = &mic->data[count-1];

		/* input component collection header */
		temp_16 = read_buffer(buffer,2);
		mic_data->input_count = temp_16 & 0x1FFF;
		mic_data->input_component_type = temp_16 >> 14;
		length-=2;

		/* input component collection data */
		temp_16 = mic_data->input_count * (1<<mic_data->input_component_type);
		length-=temp_16;
		mic_data->input_components = (unsigned char*)calloc(temp_16, sizeof(unsigned char));
		for(i=0; i<temp_16; ++i) {
			mic_data->input_components[i] = read_byte(buffer);
		}

		/* output component collection header */
		temp_16 = read_buffer(buffer,2);
		mic_data->output_count = temp_16 & 0x1FFF;
		mic_data->output_component_type = temp_16 >> 14;
		length-=2;

		/* output component collection data */
		temp_16 = mic_data->output_count * (1<<mic_data->output_component_type);
		length-=temp_16;
		mic_data->output_components = (unsigned char*)calloc(temp_16, sizeof(unsigned char));
		for(i=0; i<temp_16; ++i) {
			mic_data->output_components[i] = read_byte(buffer);
		}

		/* component collection footer */
		temp_8 = read_byte(buffer);
		mic_data->decorrelation_transform_matrix = temp_8 & 0xF;
		mic_data->deccorelation_transform_offset = temp_8 >> 4;
		length-=1;
	}
	mic->count = count;
}


/**
 * @brief Reads ATK marker.
 *
 * @param buffer
 * @param img
*/ 
void read_atk_marker(type_buffer *buffer, type_image *img) {
	int marker;
	int length;
	int i;
	int count;
	unsigned short temp;
	unsigned char* t;
	type_atk* atk;
	type_atk* old_atks = img->mct_data->atks;
	img->mct_data->atks = (type_atk*)realloc(img->mct_data->atks, sizeof(type_atk) * (++img->mct_data->atk_count));
	atk = &img->mct_data->atks[img->mct_data->atk_count-1];

	if(img->mct_data->atks == NULL) {
		img->mct_data->atks = old_atks;
		--img->mct_data->atk_count;
		println_var(INFO, "Error: Memory reallocation failed! Aborting read of ATK segment");
		return;
	}


	/* Read ATK Marker */
	marker = read_buffer(buffer, 2);
	if(marker != ATK)
	{
		println_var(INFO, "Error: Expected ATK marker instead of %x", marker);
	}
	length = read_buffer(buffer, 4)-5;

	temp = read_byte(buffer);
	atk->index = temp & 0xF;
	atk->coeff_type = (temp & 0x0070) >> 4;
	atk->filter_category = (temp & 0x0180) >> 7;
	atk->wavelet_type = (temp & 0x0200) >> 9;
	atk->m0 = (temp & 0x400) >> 10;

	atk->lifing_steps = read_byte(buffer);
	atk->lifting_coefficients_per_step = read_byte(buffer);
	atk->lifting_offset = read_byte(buffer);

	count = 1<<atk->coeff_type;
	t = (unsigned char*)calloc(count, sizeof(unsigned char));
	for(i=0; i<count; ++i) {
		t[i]=read_byte(buffer);
	}

	if(atk->wavelet_type == 1) {
		atk->scaling_exponent = t;
	} else {
		atk->scaling_factor = t;
	}

	count = atk->lifing_steps * atk->lifting_coefficients_per_step * atk->wavelet_type?1:(1<<atk->coeff_type);
	atk->coefficients = (unsigned char*)calloc(count, sizeof(unsigned char));
	for(i=0; i<count; ++i) {
		atk->coefficients[i] = read_byte(buffer);
	}

	if(atk->wavelet_type == 1) {
		count = atk->lifing_steps * (1<<atk->coeff_type);
		atk->additive_residue = (unsigned char*)calloc(count, sizeof(unsigned char));
		for(i=0; i<count; ++i) {
			atk->additive_residue[i] = read_byte(buffer);
		}
	}
}


/**
 * @brief Reads ADS marker.
 *
 * @param buffer
 * @param img
*/ 
void read_ads_marker(type_buffer *buffer, type_image *img) {
	int marker;
	int length;
	int i;
	unsigned char temp;
	type_ads *old_adses, *ads;

	old_adses = img->mct_data->adses;
	img->mct_data->adses = (type_ads*)realloc(img->mct_data->adses, sizeof(type_ads) * (++img->mct_data->ads_count));
	ads = &img->mct_data->adses[img->mct_data->ads_count-1];

	if(img->mct_data->adses == NULL) {
		img->mct_data->adses = old_adses;
		--img->mct_data->ads_count;
		println_var(INFO, "Error: Memory reallocation failed! Aborting read of ADS segment");
		return;
	}


	/* Read ADS Marker */
	marker = read_buffer(buffer, 2);
	if(marker != ADS)
	{
		println_var(INFO, "Error: Expected ADS marker instead of %x", marker);
	}
	length = read_buffer(buffer, 4)-5;
	ads->index = read_byte(buffer);
	ads->IOads = read_byte(buffer);
	ads->DOads = (unsigned char*)calloc(ads->IOads, sizeof(unsigned char));	
	i = 0;
	while(ads->DOads - i >0) {
		if(i%4 == 0)
			temp = read_byte(buffer);
		ads->DOads[i] = (temp & (3 << (3-i)*2)) >> (3-i)*2;
		++i;
	}

	ads->ISads = read_byte(buffer);
	ads->DSads = (unsigned char*)calloc(ads->ISads, sizeof(unsigned char));	
	i = 0;
	while(ads->DSads - i >0) {
		if(i%4 == 0)
			temp = read_byte(buffer);
		ads->DSads[i] = (temp & (3 << (3-i)*2)) >> (3-i)*2;
		++i;
	}
}


/** Reads all data needed for performing multiple component transformations as in 15444-2 from codestream */
void read_multiple_component_transformations(type_buffer *buffer, type_image *img) {
	img->mct_data = (type_multiple_component_transformations*)calloc(1, sizeof(type_multiple_component_transformations));
	while(peek_marker(buffer)==ADS) {
		read_ads_marker(buffer, img);
	}
	while(peek_marker(buffer)==ATK) {
		read_atk_marker(buffer, img);
	}
	while(peek_marker(buffer)==MCT) {
		read_mct_marker(buffer, img);
	}
	while(peek_marker(buffer)==MCC) {
		read_mcc_marker(buffer, img);
	}
	while(peek_marker(buffer)==MIC) {
		read_mic_marker(buffer, img);
	}
}


