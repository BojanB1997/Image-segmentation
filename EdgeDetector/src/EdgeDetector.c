/*****************************************************************************
 * EdgeDetector.c
 *****************************************************************************/

#include <sys/platform.h>
#include <def21489.h>
#include <sru21489.h>
#include <SYSREG.h>
#include <cycle_count.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "adi_initialize.h"
#include <math.h>
#include "EdgeDetector.h"

#define DATA_OFFSET_OFFSET 0x000A
#define WIDTH_OFFSET 0x0012
#define HEIGHT_OFFSET 0x0016
#define BITS_PER_PIXEL_OFFSET 0x001C
#define HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define NO_COMPRESION 0
#define MAX_NUMBER_OF_COLORS 0
#define ALL_COLORS_REQUIRED 0


typedef struct {
    int type;
    int size;
    int reserved1;
    int reserved2;
    int offset;
    int header_size;
    int width;
    int height;
    int planes;
    int bits_per_pixel;
    int compression;
    int image_size;
    int x_pixels_per_meter;
    int y_pixels_per_meter;
    int colors_used;
    int colors_important;
} bmp_header_t;

#pragma section("seg_sdram1")
unsigned int pixels[250000];

#pragma section("seg_sdram1")
unsigned int outputPixels[250000];

#pragma section("seg_sdram1")
int mask_x[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1} };

#pragma section("seg_sdram1")
int mask_y[3][3] = { {-1, -2, -1}, {0, 0, 0}, {1, 2, 1} };


void init(void);
void read_image(bmp_header_t*, FILE*);
void write_image(bmp_header_t*, FILE*, int*);
void convolve(int*, int, int);
void black_and_white(int*, int);
void code_pixels(int*, int, int);
void code_image(int* pixels, int height, int width);

cycle_t start_count;
cycle_t final_count;

int main(int argc, char *argv[])
{
	init();

	//turn off LEDs
	sysreg_bit_clr(sysreg_FLAGS, FLG4);
	sysreg_bit_clr(sysreg_FLAGS, FLG5);
	sysreg_bit_clr(sysreg_FLAGS, FLG6);
	SRU(LOW,DAI_PB03_I);
	SRU(LOW,DAI_PB04_I);
	SRU(LOW,DAI_PB15_I);
	SRU(LOW,DAI_PB16_I);
	SRU(LOW,DAI_PB17_I);

	bmp_header_t bmp_header;
	sysreg_bit_set(sysreg_FLAGS, FLG4);

	START_CYCLE_COUNT(start_count);
	FILE* in = fopen("img.bmp", "r");
	read_image(&bmp_header, in);
	STOP_CYCLE_COUNT(final_count, start_count);
	PRINT_CYCLES("Ucitavanje slike: ", final_count);
	fclose(in);
	sysreg_bit_set(sysreg_FLAGS, FLG5);


	unsigned char tempRGB;
	int rgbIdx;
	int i, temp = 0;

	START_CYCLE_COUNT(start_count);
	for (rgbIdx = 0; rgbIdx < (bmp_header.image_size/3); rgbIdx++) {
	    temp = (pixels[rgbIdx] & 0x000000FF) * 0.3 + ((pixels[rgbIdx] >> 8) & 0x000000FF) * 0.59 + ((pixels[rgbIdx] >> 16) & 0x000000FF) * 0.11;
	    pixels[rgbIdx] = 0;
	    pixels[rgbIdx] = temp;
	    pixels[rgbIdx] = (pixels[rgbIdx] << 8) | temp;
	    pixels[rgbIdx] = (pixels[rgbIdx] << 8) | temp;
	}
	STOP_CYCLE_COUNT(final_count, start_count);
	PRINT_CYCLES("Gray scale: ", final_count);

	sysreg_bit_set(sysreg_FLAGS, FLG6);

	START_CYCLE_COUNT(start_count);
	convolve(pixels, bmp_header.height, bmp_header.width);
	STOP_CYCLE_COUNT(final_count, start_count);
	PRINT_CYCLES("Sobelov algoritam: ", final_count);

	SRU(HIGH,DAI_PB03_I);

	START_CYCLE_COUNT(start_count);
	black_and_white(outputPixels, bmp_header.image_size);
	STOP_CYCLE_COUNT(final_count, start_count);
	PRINT_CYCLES("Pretvaranje u crno bijelo: ", final_count);

	SRU(HIGH,DAI_PB04_I);

	START_CYCLE_COUNT(start_count);
	FILE *out = fopen( "img_edge.bmp","w");
	write_image(&bmp_header, out, outputPixels);
	STOP_CYCLE_COUNT(final_count, start_count);
	PRINT_CYCLES("Upis slike detektovanih ivica: ", final_count);
	fclose(out);

	START_CYCLE_COUNT(start_count);
	code_pixels(outputPixels, bmp_header.height, bmp_header.width);
	STOP_CYCLE_COUNT(final_count, start_count);
	PRINT_CYCLES("Kodovanje bojom: ", final_count);

	SRU(HIGH,DAI_PB15_I);
	printf("Zavrsio sam prvi!\n");


	START_CYCLE_COUNT(start_count);
	out = fopen( "img_obojen.bmp","w");
	write_image(&bmp_header, out, outputPixels);
	STOP_CYCLE_COUNT(final_count, start_count);
	PRINT_CYCLES("Upis kodovane slike: ", final_count);

	SRU(HIGH,DAI_PB16_I);
	fclose(out);

	printf("Zavrsio sam drugi!\n");
	SRU(HIGH,DAI_PB17_I);
	return 0;
}

void read_image(bmp_header_t* bmp_header, FILE* in){
	int b1, b2, b3, b4;
	int i = 0;

	/* CITANJE TIPA */
	b1 = fgetc(in);
	b2 = fgetc(in);
	bmp_header->type = b2;
	bmp_header->type = bmp_header->type << 8;
	bmp_header->type = bmp_header->type | b1;

	printf("Tip: %x\n", bmp_header->type);

	/* CITANJE VELICINE FAJLA */
	b1 = fgetc(in);
	b2 = fgetc(in);
	b3 = fgetc(in);
	bmp_header->size = (((fgetc(in) << 24) | (b3 << 16)) | (b2 << 8)) | b1;

	printf("Velicina fajla: %d\n", bmp_header->size);

	/* PRESKAKANJE REZERVISANIH BAJTOVA */
	b1 = fgetc(in);
	bmp_header->reserved1 = (fgetc(in) << 8) | b1;
	b1 = fgetc(in);
	bmp_header->reserved2 = (fgetc(in) << 8) | b1;

	/* CITANJE OFFSETA */
	b1 = fgetc(in);
	b2 = fgetc(in);
	b3 = fgetc(in);
	bmp_header->offset = (((fgetc(in) << 24) | (b3 << 16)) | (b2 << 8)) | b1;

	/* CITANJE VELICINE HEDERA */
	b1 = fgetc(in);
	b2 = fgetc(in);
	b3 = fgetc(in);
	bmp_header->header_size = (((fgetc(in) << 24) | (b3 << 16)) | (b2 << 8)) | b1;

	/* CITANJE DIMENZIJA SLIKE */
	b1 = fgetc(in);
	b2 = fgetc(in);
	b3 = fgetc(in);
	bmp_header->width = (((fgetc(in) << 24) | (b3 << 16)) | (b2 << 8)) | b1;

	b1 = fgetc(in);
	b2 = fgetc(in);
	b3 = fgetc(in);
	bmp_header->height = (((fgetc(in) << 24) | (b3 << 16)) | (b2 << 8)) | b1;

	printf("Dimenzije slike su: %d x %d\n", bmp_header->width, bmp_header->height);

	/* PLANES */
	b1 = fgetc(in);
	bmp_header->planes = (fgetc(in) << 8) | b1;

	/* BITS PER PIXEL */
	b1 = fgetc(in);
	bmp_header->bits_per_pixel = (fgetc(in) << 8) | b1;

	/* COMPRESSION */
	b1 = fgetc(in);
	b2 = fgetc(in);
	b3 = fgetc(in);
	bmp_header->compression = (((fgetc(in) << 24) | (b3 << 16)) | (b2 << 8)) | b1;

	/* IMAGE SIZE */
	b1 = fgetc(in);
	b2 = fgetc(in);
	b3 = fgetc(in);
	bmp_header->image_size = (((fgetc(in) << 24) | (b3 << 16)) | (b2 << 8)) | b1;
	printf("Velicina slike: %d\n", bmp_header->image_size);

	/* X/Y PIXELS PER METER */
	b1 = fgetc(in);
	b2 = fgetc(in);
	b3 = fgetc(in);
	bmp_header->x_pixels_per_meter = (((fgetc(in) << 24) | (b3 << 16)) | (b2 << 8)) | b1;

	b1 = fgetc(in);
	b2 = fgetc(in);
	b3 = fgetc(in);
	bmp_header->y_pixels_per_meter = (((fgetc(in) << 24) | (b3 << 16)) | (b2 << 8)) | b1;

	/* COLORS USED */
	b1 = fgetc(in);
	b2 = fgetc(in);
	b3 = fgetc(in);
	bmp_header->colors_used = (((fgetc(in) << 24) | (b3 << 16)) | (b2 << 8)) | b1;

	/* COLORS IMPORTANT */
	b1 = fgetc(in);
	b2 = fgetc(in);
	b3 = fgetc(in);
	bmp_header->colors_important = (((fgetc(in) << 24) | (b3 << 16)) | (b2 << 8)) | b1;

	/* READ PIXELS */
	printf("Pocinjem citanje piksela.\n");

	for(i = 0; i < (bmp_header->image_size)/3; i++){
		b1 = fgetc(in);
		b2 = fgetc(in);
		pixels[i] = ((fgetc(in) << 16) | (b2 << 8)) | b1;
	}
}

void write_image(bmp_header_t* bmp_header, FILE* out, int* pixels){
	int i;
    fputc(bmp_header->type, out);
    fputc((bmp_header->type >> 8), out);
    fputc(bmp_header->size, out);
    fputc((bmp_header->size >> 8), out);
    fputc((bmp_header->size >> 16), out);
    fputc((bmp_header->size >> 24), out);
    fputc(bmp_header->reserved1, out);
    fputc((bmp_header->reserved1 >> 8), out);
    fputc(bmp_header->reserved2, out);
    fputc((bmp_header->reserved2 >> 8), out);
    fputc(bmp_header->offset, out);
    fputc((bmp_header->offset >> 8), out);
    fputc((bmp_header->offset >> 16), out);
    fputc((bmp_header->offset >> 24), out);
    fputc(bmp_header->header_size, out);
    fputc((bmp_header->header_size >> 8), out);
    fputc((bmp_header->header_size >> 16), out);
    fputc((bmp_header->header_size >> 24), out);
    fputc(bmp_header->width, out);
    fputc((bmp_header->width >> 8), out);
    fputc((bmp_header->width >> 16), out);
    fputc((bmp_header->width >> 24), out);
    fputc(bmp_header->height, out);
    fputc((bmp_header->height >> 8), out);
    fputc((bmp_header->height >> 16), out);
    fputc((bmp_header->height >> 24), out);
    fputc(bmp_header->planes, out);
    fputc((bmp_header->planes >> 8), out);
    fputc(bmp_header->bits_per_pixel, out);
    fputc((bmp_header->bits_per_pixel >> 8), out);
    fputc(bmp_header->compression, out);
    fputc((bmp_header->compression >> 8), out);
    fputc((bmp_header->compression >> 16), out);
    fputc((bmp_header->compression >> 24), out);
    fputc(bmp_header->image_size, out);
    fputc((bmp_header->image_size >> 8), out);
    fputc((bmp_header->image_size >> 16), out);
    fputc((bmp_header->image_size >> 24), out);
    fputc(bmp_header->x_pixels_per_meter, out);
    fputc((bmp_header->x_pixels_per_meter >> 8), out);
    fputc((bmp_header->x_pixels_per_meter >> 16), out);
    fputc((bmp_header->x_pixels_per_meter >> 24), out);
    fputc(bmp_header->y_pixels_per_meter, out);
    fputc((bmp_header->y_pixels_per_meter >> 8), out);
    fputc((bmp_header->y_pixels_per_meter >> 16), out);
    fputc((bmp_header->y_pixels_per_meter >> 24), out);
    fputc(bmp_header->colors_used, out);
    fputc((bmp_header->colors_used >> 8), out);
    fputc((bmp_header->colors_used >> 16), out);
    fputc((bmp_header->colors_used >> 24), out);
    fputc(bmp_header->colors_important, out);
    fputc((bmp_header->colors_important >> 8), out);
    fputc((bmp_header->colors_important >> 16), out);
    fputc((bmp_header->colors_important >> 24), out);

    for(i = 0; i < (bmp_header->image_size)/3; i++){
    	fputc(pixels[i], out);
    	fputc((pixels[i] >> 8), out);
    	fputc((pixels[i] >> 16), out);
    }
    printf("Zavrsio pisanje piksela.\n");
}

void convolve(int* pixels, int height, int width){
	int i, j, sum_x, sum_y, pixIndex;
	int susjed1, susjed2, susjed3, susjed4, susjed5, susjed6, susjed7, susjed8;

	for (i = 1; i < width - 1; i++) {
		for (j = 1; j < height - 1; j++) {
			pixIndex = i * height + j;
	        susjed1 = pixIndex - (width + 1);
	        susjed2 = pixIndex - width;
	        susjed3 = pixIndex - (width - 1);
	        susjed4 = pixIndex - 1;
	        susjed5 = pixIndex + 1;
	        susjed6 = pixIndex + (width - 1);
	        susjed7 = pixIndex + width;
	        susjed8 = pixIndex + (width + 1);

	        sum_x = pixels[susjed1] * mask_x[0][0] + pixels[susjed2] * mask_x[0][1] + pixels[susjed3] * mask_x[0][2] +
	                pixels[susjed4] * mask_x[1][0] + pixels[pixIndex] * mask_x[1][1] + pixels[susjed5] * mask_x[1][2] +
	                pixels[susjed6] * mask_x[2][0] + pixels[susjed7] * mask_x[2][1] + pixels[susjed8] * mask_x[2][2];

	        sum_y = pixels[susjed1] * mask_y[0][0] + pixels[susjed2] * mask_y[0][1] + pixels[susjed3] * mask_y[0][2] +
	                pixels[susjed4] * mask_y[1][0] + pixels[pixIndex] * mask_y[1][1] + pixels[susjed5] * mask_y[1][2] +
	                pixels[susjed6] * mask_y[2][0] + pixels[susjed7] * mask_y[2][1] + pixels[susjed8] * mask_y[2][2];

	        outputPixels[pixIndex] = abs(sum_x) + abs(sum_y);
	        }
	    }

	for (i = 0; i < width; i++) {
		pixIndex = i;
	    outputPixels[pixIndex] = 0x00FFFFFF;

	    pixIndex = (height - 1)*(width) + i;
	    outputPixels[pixIndex] = 0x00FFFFFF;
	}


	for (i = 1; i <height - 1; i++) {
		pixIndex = i * width;
	    outputPixels[pixIndex] = 0x00FFFFFF;

	    pixIndex = (i + 1) * width - 1;
	    outputPixels[pixIndex] = 0x00FFFFFF;
	}

}

void black_and_white(int* pixels, int size){
	int i;

	for(i = 0; i < size/3; i++){
		if(pixels[i] < 1){
			pixels[i] = 0x00FFFFFF;
		}
		else{
			pixels[i] = 0;
		}
	}
}

void code_pixels(int* pixels, int height, int width){
    printf("Usao u kodovanj\n");
    int i, j, pixIndex, pixAbove;
    int codeValue = 0x000000A0;

    for (i = 0; i < width; i++) {
        for (j = 0; j < height; j++) {
            pixIndex = i * height + j;
            pixAbove = (i - 1) * height + j;
            if (pixels[pixIndex] == 0) {
                pixels[pixIndex] = 0;
            }
            else if (pixels[pixIndex - 1] == 0 && pixels[pixAbove] == 0 && pixels[pixAbove + 1] != 0 && pixels[pixIndex] == 0x00FFFFFF) {
            	pixels[pixIndex] = pixels[pixAbove + 1];
            }
            else if (pixels[pixIndex - 1] == 0 && pixels[pixAbove] == 0 && pixels[pixIndex] == 0x00FFFFFF ) {
                pixels[pixIndex] = codeValue;
                codeValue = codeValue << 5;
            }
            else if(pixels[pixIndex] == 0x00FFFFFF && pixels[pixAbove] != 0){
            	pixels[pixIndex] = pixels[pixAbove];
            }
            else if(pixels[pixIndex] == 0x00FFFFFF && pixels[pixIndex - 1] != 0){
            	pixels[pixIndex] = pixels[pixIndex - 1];
            }
        }
    }
}

void init(void)
{
	//** LED01**//
	SRU(HIGH,DPI_PBEN06_I);
	SRU(FLAG4_O,DPI_PB06_I);
	//** LED02**//
	SRU(HIGH,DPI_PBEN13_I);
	SRU(FLAG5_O,DPI_PB13_I);
	//** LED03**//
	SRU(HIGH,DPI_PBEN14_I);
	SRU(FLAG6_O,DPI_PB14_I);
	//** LED04**//
	SRU(HIGH,DAI_PBEN03_I);
	SRU(HIGH,DAI_PB03_I);
	//** LED05**//
	SRU(HIGH,DAI_PBEN04_I);
	SRU(HIGH,DAI_PB04_I);
	//** LED06**//
	SRU(HIGH,DAI_PBEN15_I);
	SRU(HIGH,DAI_PB15_I);
	//** LED07**//
	SRU(HIGH,DAI_PBEN16_I);
	SRU(HIGH,DAI_PB16_I);
	//** LED08**//
	SRU(HIGH,DAI_PBEN17_I);
	SRU(HIGH,DAI_PB17_I);
	//Setting flag pins as outputs
	sysreg_bit_set(sysreg_FLAGS, (FLG4O|FLG5O|FLG6O) );
	//Setting HIGH to flag pins
	sysreg_bit_set(sysreg_FLAGS, (FLG4|FLG5|FLG6) );
}

