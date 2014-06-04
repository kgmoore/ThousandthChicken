#include "boxes.h"
#include <stdlib.h>
#include <string.h>
#include "logger.h"
#include "io_buffered_stream.h"
#include "codestream.h"



box *init_box() {
	box *newBox = (box*)malloc(sizeof(box));
	memset(newBox, 0, sizeof(box));
	newBox->lbox = (unsigned char *)malloc(sizeof(char) * 5);
	newBox->tbox = (unsigned char *)malloc(sizeof(char) * 5);
	return newBox;
}

//dest has to be n+1 long
char *sstrncpy(char *dest, const char *src, size_t n) {
	println_start(INFO);
	memcpy(dest, src, n);
	println(INFO, "middle");
	dest[n] = '\0';
	return dest;
}


long int hex_to_long(unsigned char *hex, int length) {
	long int ret = hex[length-1];
	long tmp = 1;
	int i;
//	printf("1 * %x\n", hex[length-1]);
	for(i = 2; i <= length; i++) {
		tmp *= 256;
//		printf("%li * %x\n", tmp, hex[length-i]);
		ret += tmp*hex[length-i];
	}
	return ret;
}

unsigned char *read_bytes(unsigned char *dest, FILE *fd, long int n) {
	size_t bytesRead = 0;
	size_t bytesRemaining = n;
	unsigned char* target = dest;
	while ( bytesRemaining > 0 && (bytesRead = fread(target, 1, bytesRemaining, fd)) != 0 )
	{
		bytesRemaining -= bytesRead;
		target += bytesRead;
	}
	if (bytesRemaining != 0)
		return NULL;
	dest[n] = '\0';
	return (unsigned char *)dest;
}


box *get_next_box(FILE *fd) {
	int read = 0;
	box* box;
	println_start(INFO);
	box = init_box();
	if( (box->lbox = read_bytes(box->lbox, fd, 4)) == NULL)
		return NULL;
	box->length = hex_to_long(box->lbox, 4);
	if(box->length == 0) {
		return NULL;
	}
	if( (box->tbox = read_bytes(box->tbox, fd, 4)) == NULL) {
		println(INFO, "Corrupted JP2 file. Exiting.");
		return NULL;
	}
	read = 8;

	if(box->length == 1) { //there should be XLbox field present;
		box->xlbox = (unsigned char*)malloc(9 * sizeof(char));
		box->xlbox = read_bytes(box->xlbox, fd, 8);
		box->length = hex_to_long(box->xlbox, 8);
		read += 8;
	}
	box->content_length = box->length - read;

	box->dbox = (unsigned char*)malloc((box->content_length + 1) * sizeof(char));
	box->dbox = read_bytes(box->dbox, fd, box->content_length);
	println_end(INFO);
	return box;
}

box *get_next_box_char(box *superbox) {
	char *content;
	box* nextBox;
	int read,size;
	if(superbox->read >= superbox->content_length)
		return NULL;

	content = (char*)superbox->dbox;
	nextBox = (box*)malloc(sizeof(box));

	read = superbox->read;
	nextBox->lbox = (unsigned char*)malloc(5 * sizeof(char));
	nextBox->lbox = (unsigned char*)sstrncpy((char*)nextBox->lbox, &content[read], 4);
	read += 4;
	nextBox->length = hex_to_long(nextBox->lbox, 4);

	println_var(INFO, "lbox: %i", nextBox->length);
	nextBox->tbox = (unsigned char*)malloc(5 * sizeof(char));
	nextBox->tbox =  (unsigned char*)sstrncpy((char*)nextBox->tbox, &content[read], 4);
	read += 4;

	println(INFO, "tbox");
	//box->length = hex_to_long(box->lbox, 4);
//	println_var(INFO, "length: %i", box->length);
	if(nextBox->length == 1) { //there should be XLbox field present;
		nextBox->xlbox =  (unsigned char*)malloc(9 * sizeof(char));
		nextBox->xlbox =  (unsigned char*)sstrncpy((char*)nextBox->xlbox, &content[read], 8);
		nextBox->length = hex_to_long(nextBox->xlbox, 8);
		read += 8;
	}

	size = (nextBox->length - (read - superbox->read));
	nextBox->dbox =  (unsigned char*)malloc((size+1) * sizeof(char));
	nextBox->dbox =  (unsigned char*)sstrncpy((char*)nextBox->dbox, &content[read], size);
	read += size;

	nextBox->content_length = nextBox->length - read;
	println(INFO, "dbox");
	superbox->read = read;
	println_end(INFO);
	return nextBox;
}

int dispose_of(box *b) {
	if (!b)
		return -1;
	println_start(INFO);
	if(b->lbox == NULL)
		printf("NULL!\n");
	else
		free(b->lbox);
	println(INFO, "A");

	if (b->tbox)
	   free(b->tbox);
	if (b->dbox)
	   free(b->dbox);
	if(b->xlbox)
		free(b->xlbox);
	free(b);
	return 0;
}

int h_filetype_box(box *b, type_image *img) {
	char minv[5];
	char cl[5];
	char br[5];
	int left,i;
	sstrncpy(br, (const char*)b->dbox, 4);

	if(strcmp(br, "jp2\040")) {
		println(INFO, "DOSEN'T Conform to IS 15444-1. Exiting");
		return 1;
	} else
		println(INFO, "Conforms to IS 15444-1");

	sstrncpy(minv, (const char*)&(b->dbox[4]), 4);

	if(hex_to_long( (unsigned char*)minv, 4) != 0) {
		println(INFO, "MinV should be 0");
	} else
		println(INFO, "MinV OK");

	left = b->content_length - 8; //16 bytes already read from the box's contents: br,minv
	printf("left: %i\n", left);
	i = 1;
	while(left) {
		sstrncpy(cl, (const char*)&(b->dbox)[4 + i*4], 4); 
		left -= 4;
		i++;

		//TODO: filetype box: codestream profile restrictions
		if(!strcmp(cl, "J2P0")) {
			println(INFO, "First codestream is restricted: Profile-0");
		} else
		if(!strcmp(cl, "J2P1")) {
			println(INFO, "First codestream is restricted: Profile-1");
		} else
		if(!strcmp(cl, "jp2\040")) {
			println(INFO, "One CL field with correct fingerprint");
		} else
			println_var(INFO, "CL: >%s<", cl);
	}
	return 0;
}

int h_image_header_box(box *b, type_image *img) {
	char cwidth[5], cheight[5], cnum_comp[3];

	sstrncpy(cheight, (const char*)b->dbox, 4);
	img->height = hex_to_long((unsigned char*)cheight, 4);

	sstrncpy(cwidth, (const char*)&(b->dbox)[4], 4);
	img->width = (unsigned short)hex_to_long((unsigned char*)cwidth, 4);

	sstrncpy(cnum_comp, (const char*)&(b->dbox)[8], 2);
	img->num_components = (unsigned short)hex_to_long((unsigned char*)cnum_comp, 2);

	//TODO: bpc

	if(b->dbox[11] != 7) {
		println_var(INFO, "Value of the 'Compression type' shall be 7, not: %i", b->dbox[11]);
	}

	if(b->dbox[12] ==  0) {
		println(INFO, "Colorspace known. Specified in Colorspace box");
	} else
	if(b->dbox[12] == 1) {
		println(INFO, "Colorspace UNKNOWN.");
	}
	return 0;
}

int h_header_box(box *header_box, type_image *img) {
	box *ihdr, *b;
	println_start(INFO);
	header_box->read = 0;
	ihdr = get_next_box_char(header_box); //TODO: extract box from within b

	if(hex_to_long(ihdr->tbox, 4) != IMAGE_HEADER_BOX) {
		println(INFO, "Image Header Box should be the first one in Header superbox. Exitting!");
		return 1;
	} else
		h_image_header_box(ihdr, img);

	while( (b = get_next_box_char(header_box)) != NULL ) {
			if(hex_to_long(b->tbox, 4) == BITS_PER_COMPONENT_BOX) {
				println(INFO, "Bits Per Component box");
			} else
			if(hex_to_long(b->tbox,4) ==  COLOR_BOX) {
				println(INFO, "Color Specification Box");
			} else
			if(hex_to_long(b->tbox,4) ==  PALETTE_BOX) {
				println(INFO, "Palette Box");
			} else
			if(hex_to_long(b->tbox,4) ==  COMPONENT_MAPPING_BOX) {
				println(INFO, "Component Mapping Box");
			} else
			if(hex_to_long(b->tbox,4) ==  CHANNEL_DEFINITION_BOX) {
				println(INFO, "Channel Definition Box");
			}
	}

	return 0;
}

void h_contiguous_codestream_box(box *cbox, type_image *img) {

	long int codestream_len = cbox->content_length;

	type_buffer *src_buff = (type_buffer *) malloc(sizeof(type_buffer));
	src_buff->data = (unsigned char *) malloc(codestream_len+1);
	src_buff->size = codestream_len;
	src_buff->data = cbox->dbox;
	src_buff->start = src_buff->data;
	src_buff->end = src_buff->data + src_buff->size;
	src_buff->bp = src_buff->data;
	src_buff->bits_count = 0;
	src_buff->byte = 0;

	println(INFO, "Decoding codestream");
	decode_codestream(src_buff, img);

	println_end(INFO);
}

int jp2_parse_boxes(FILE *fd, type_image *img) {
	box *sig = get_next_box(fd);
	box *ft=NULL, *b=NULL;

	if(hex_to_long(sig->tbox, 4) != JP2_SIGNATURE_BOX)
		println(INFO, "JP2 signature box should be very first box in the file: Header");

	if(hex_to_long(sig->dbox, 4) != JP2_SIG_BOX_CONTENT)
		println(INFO, "JP2 signature box should be very first box in the file: Content");

	if (sig)
	  dispose_of(sig);
	sig = NULL;
	ft = get_next_box(fd);

	if(hex_to_long(ft->tbox, 4) != JP2_FILETYPE_BOX)
		println(INFO, "JP2 filetype box should directly follow JP2 signature box");

	if(h_filetype_box(ft, img))
		return 1;
	if (ft)
	   dispose_of(ft);
	ft = NULL;

	while( (b = get_next_box(fd)) != NULL ) {
		if(hex_to_long(b->tbox, 4) == JP2_HEADER_BOX) {
			println(INFO, "Header Box");
			h_header_box(b,img);
		} else
		if(hex_to_long(b->tbox,4) ==  CODE_STREAM_BOX) {
			println(INFO, "Contiguous Codestream Box");
			h_contiguous_codestream_box(b,img);

		} else
		if(hex_to_long(b->tbox,4) ==  INTELLECTUAL_PROPERTY_BOX) {
			println(INFO, "Intellectual Property Box");
		}
		dispose_of(b);
	}
	b = NULL;
	println_end(INFO);
	return 0;
}
