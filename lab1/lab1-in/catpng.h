#pragma once

/******************************************************************************
 * INCLUDE HEADER FILES
 *****************************************************************************/

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>   /* for printf().  man 3 printf */
#include <stdlib.h>  /* for exit().    man 3 exit   */
#include <string.h>  /* for strcat().  man strcat   */
#include <sys/types.h>
#include "lab_png.h"   /* simple PNG data structures  */
#include <errno.h>
#include "crc.h"      /* for crc()                   */
#include "arpa/inet.h" /* for htonl()                 */
#include "zutil.h"    /* for mem_def() and mem_inf() */

/******************************************************************************
 * FUNCTION PROTOTYPES
 *****************************************************************************/
int uncompress_IDAT_image_data(struct PNG_file_data *compressed_png_file,
                               struct IDAT_uncompressed_data *compressed_png_info);

int png_files_same_width(struct PNG_file_data *png_file,
                         int total_number_of_images);

void calculate_concatenated_length_and_height(U64 *total_concatenated_length,
                                              U32 *total_concatenated_height,
                                              int total_number_of_images,
                                              struct PNG_file_data *png_file,
                                              struct IDAT_uncompressed_data *uncompressed_png_info);

U8 * concatenate_uncompressed_png_images(struct IDAT_uncompressed_data *uncompressed_png_info,
                                         U64 concatenate_length,
                                         int total_number_of_images);

U8 * concatenate_compressed_IDAT(U64 *concatenated_png_length,
                                 U8 *uncompressed_data,
                                 U64 uncompressed_data_length);

int populate_IHDR_png_chunk(struct PNG_file_data *png_file,
                            struct PNG_file_data *original_png_file,
                            U8 *IHDR_buf,
                            U32 image_height);

int populate_IDAT_png_chunk(struct chunk *out,
                            U8 *IDAT_compressed_data,
                            U64 IDAT_length);

int populate_IEND_png_chunk(struct chunk *out);

int write_png_chunk(struct chunk *out,
                      FILE *fs);

int create_new_png(struct PNG_file_data *new_png_file,
                   struct PNG_file_data *old_png_file,
                   U8 *concated_IDAT_data,
                   U64 new_png_file_length,
                   U32 new_png_height,
                   char *file_name);
