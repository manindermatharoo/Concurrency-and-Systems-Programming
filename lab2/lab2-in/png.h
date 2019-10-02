/**
 * @brief  micros and structures for a simple PNG file 
 *
 * Copyright 2018-2019 Yiqing Huang
 *
 * This software may be freely redistributed under the terms of MIT License
 */
#pragma once

/******************************************************************************
 * INCLUDE HEADER FILES
 *****************************************************************************/
#include <stdio.h>

/******************************************************************************
 * DEFINED MACROS 
 *****************************************************************************/

#define PNG_SIG_SIZE    8 /* number of bytes of png image signature data */
#define CHUNK_LEN_SIZE  4 /* chunk length field size in bytes */          
#define CHUNK_TYPE_SIZE 4 /* chunk type field size in bytes */
#define CHUNK_CRC_SIZE  4 /* chunk CRC field size in bytes */
#define DATA_IHDR_SIZE 13 /* IHDR chunk data field size */

/******************************************************************************
 * STRUCTURES and TYPEDEFS 
 *****************************************************************************/
typedef unsigned char U8;
typedef unsigned int  U32;
typedef unsigned long int U64;

typedef struct chunk {
    U32 length;  /* length of data in the chunk, host byte order */
    U8  type[4]; /* chunk type */
    U8  *p_data; /* pointer to location where the actual data are */
    U32 crc;     /* CRC field  */
} *chunk_p;

/* note that there are 13 Bytes valid data, compiler will padd 3 bytes to make
   the structure 16 Bytes due to alignment. So do not use the size of this
   structure as the actual data size, use 13 Bytes (i.e DATA_IHDR_SIZE macro).
 */
typedef struct data_IHDR {// IHDR chunk data 
    U32 width;        /* width in pixels, big endian   */
    U32 height;       /* height in pixels, big endian  */
    U8  bit_depth;    /* num of bits per sample or per palette index.
                         valid values are: 1, 2, 4, 8, 16 */
    U8  color_type;   /* =0: Grayscale; =2: Truecolor; =3 Indexed-color
                         =4: Greyscale with alpha; =6: Truecolor with alpha */
    U8  compression;  /* only method 0 is defined for now */
    U8  filter;       /* only method 0 is defined for now */
    U8  interlace;    /* =0: no interlace; =1: Adam7 interlace */
} *data_IHDR_p;

/* A simple PNG file format, three chunks only*/
typedef struct simple_PNG {
    struct chunk *p_IHDR;
    struct chunk *p_IDAT;  /* only handles one IDAT chunk */  
    struct chunk *p_IEND;
} *simple_PNG_p;

typedef struct image_data{
    U8 *idat_uncompressed_data;
    U32 height;
    U32 width;
    U64 size;
} *image_data_p;

/******************************************************************************
 * FUNCTION PROTOTYPES 
 *****************************************************************************/
int is_png(U8 *buf, size_t n);
U32 get_png_height(struct data_IHDR *buf);
U32 get_png_width(struct data_IHDR *buf);
int get_png_data_IHDR(struct chunk *info, struct data_IHDR *out);

/* declare your own functions prototypes here */
int check_crc_value(struct chunk *out);
simple_PNG_p get_png_p();
int parse_png(simple_PNG_p png_p, char* png);
simple_PNG_p concat_pngs(int num_pngs, simple_PNG_p* pngs);
int get_png_data_IHDR(struct chunk *info, struct data_IHDR *out);
data_IHDR_p get_IHDR_data(simple_PNG_p png_p);
U32 get_png_height(struct data_IHDR *buf);
U32 get_png_width(struct data_IHDR *buf);
void compute_crc(chunk_p chunk);
int write_png_file(simple_PNG_p png_p, char* file_name);
void set_png_height(chunk_p chunk, U32 height);
int write_png_chunk(chunk_p chunk, FILE* fp);
int compare_png(simple_PNG_p png1, simple_PNG_p png2);
int is_png(U8 *buf, size_t n);