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
#include "zutil.h"    /* for mem_def() and mem_inf() */
#include "arpa/inet.h" /* for htonl()                 */

int main(int argc, char **argv)
{
    /* Ensure there is one argument*/
    if (argc == 1)
    {
        fprintf(stderr, "Usage: %s <png file>\n", argv[0]);
        exit(1);
    }

    png_image_data_p concatenated_png_images = (png_image_data_p)malloc((argc-1) * sizeof(struct png_image_data));

    for(int i = 1; i < argc; i++)
    {
        /* Open binary png file */
        FILE *png_file = fopen(argv[i], "rb");

        /* make sure the file is a valid file to open (exists) */
        if(png_file == NULL)
        {
            fprintf(stderr, "Couldn't open %s: %s\n", argv[i], strerror(errno));
            exit(1);
        }

        /* Create all variables */
        U8 *png_file_header = (U8 *)malloc(PNG_SIG_SIZE * sizeof(U8));

        simple_PNG_p png_format = (simple_PNG_p)malloc(sizeof(struct simple_PNG));
        png_format->p_IHDR = (chunk_p)malloc(sizeof(struct chunk));
        png_format->p_IDAT = (chunk_p)malloc(sizeof(struct chunk));
        png_format->p_IEND = (chunk_p)malloc(sizeof(struct chunk));

        U8 *p_IHDR_data = NULL; /* Stores data from IHDR chunk */
        U8 *p_IDAT_data = NULL; /* Stores data from IDAT chunk */
        U8 *p_IEND_data = NULL; /* Stores data from IEND chunk */

        data_IHDR_p IHDR_struct_data = (data_IHDR_p)malloc(sizeof(DATA_IHDR_SIZE));

        /* Read the first 8 bytes of png file which should be the header */
        int header_bytes = fread(png_file_header, 1, PNG_SIG_SIZE, png_file);

        /* Make sure the file is a png before preceeding */
        if(is_png(png_file_header, header_bytes) != 0)
        {
            printf("%s: Not a PNG file\n", argv[1]);
        }
        else
        {
            /* Collect all the information from each chunk */
            process_png_chunk(png_format->p_IHDR, p_IHDR_data, png_file);
            process_png_chunk(png_format->p_IDAT, p_IDAT_data, png_file);
            process_png_chunk(png_format->p_IEND, p_IEND_data, png_file);

            /* Fill out the IHDR data structure */
            get_png_data_IHDR(png_format->p_IHDR, &concatenated_png_images[i-1].IHDR_data);

            U64 length = (1 + (4*get_png_width(&concatenated_png_images[i-1].IHDR_data)))*(get_png_height(&concatenated_png_images[i-1].IHDR_data));

            concatenated_png_images[i-1].IDAT_uncompressed_data = (U8 *)malloc(sizeof(U8) * (length));
            mem_inf(concatenated_png_images[i-1].IDAT_uncompressed_data, &concatenated_png_images[i-1].IDAT_uncompressed_length, png_format->p_IDAT->p_data, png_format->p_IDAT->length);

            /* Compute CRC checks */
            check_crc_value(png_format->p_IHDR);
            check_crc_value(png_format->p_IDAT);
            check_crc_value(png_format->p_IEND);
        }

        /* Clear all dynamically allocated memory */
        free(png_file_header);

        free(p_IHDR_data);
        free(p_IDAT_data);
        free(p_IEND_data);

        free(IHDR_struct_data);

        free(png_format->p_IHDR);
        free(png_format->p_IDAT);
        free(png_format->p_IEND);
        free(png_format);

        /* Close the png file that was opened */
        fclose(png_file);
    }

    U64 total_concatenated_length = 0;
    U32 total_concatentaed_height = 0;
    for(int i = 0; i < (argc-1); i++)
    {
        total_concatenated_length += concatenated_png_images[i].IDAT_uncompressed_length;
        total_concatentaed_height += concatenated_png_images[i].IHDR_data.height;
    }

    U8 *IDAT_concatenated_uncompressed = (U8 *)malloc(total_concatenated_length * sizeof(U8));
    U64 offset = 0;
    for(int i = 0; i < (argc-1); i++)
    {
        memcpy(IDAT_concatenated_uncompressed + offset, concatenated_png_images[i].IDAT_uncompressed_data, concatenated_png_images[i].IDAT_uncompressed_length);
        offset += concatenated_png_images[i].IDAT_uncompressed_length;
    }

    U8 *IDAT_concatenated_compressed = (U8 *)malloc(total_concatenated_length * sizeof(U8));
    U64 IDAT_concatednated_compressed_length = 0;
    int ret = 0;
    ret = mem_def(IDAT_concatenated_compressed, &IDAT_concatednated_compressed_length, IDAT_concatenated_uncompressed, total_concatenated_length, Z_DEFAULT_COMPRESSION);
    if (ret == 0) { /* success */
        printf("original len = %ld, len_def = %lu\n", total_concatenated_length, IDAT_concatednated_compressed_length);
    } else { /* failure */
        fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
        return ret;
    }

    U8 png_byte_header[PNG_SIG_SIZE];
    png_byte_header[0] = 137; //89
    png_byte_header[1] = 80;  //50
    png_byte_header[2] = 78;  //4E
    png_byte_header[3] = 71;  //47
    png_byte_header[4] = 13;  //0D
    png_byte_header[5] = 10;  //0A
    png_byte_header[6] = 26;  //1A
    png_byte_header[7] = 10;  //0A

    simple_PNG_p new_png = (simple_PNG_p)malloc(sizeof(struct simple_PNG));
    new_png->p_IHDR = (chunk_p)malloc(sizeof(struct chunk));
    new_png->p_IDAT = (chunk_p)malloc(sizeof(struct chunk));
    new_png->p_IEND = (chunk_p)malloc(sizeof(struct chunk));

    data_IHDR_p new_png_IHDR_struct_data = (data_IHDR_p)malloc(sizeof(DATA_IHDR_SIZE));
    new_png_IHDR_struct_data->width = concatenated_png_images[0].IHDR_data.width;
    new_png_IHDR_struct_data->height = total_concatentaed_height;
    new_png_IHDR_struct_data->bit_depth = concatenated_png_images[0].IHDR_data.bit_depth;
    new_png_IHDR_struct_data->color_type = concatenated_png_images[0].IHDR_data.color_type;
    new_png_IHDR_struct_data->compression = concatenated_png_images[0].IHDR_data.compression;
    new_png_IHDR_struct_data->filter = concatenated_png_images[0].IHDR_data.filter;
    new_png_IHDR_struct_data->interlace = concatenated_png_images[0].IHDR_data.interlace;
    U8 temp[13];
    temp[0] = (new_png_IHDR_struct_data->width >> 24) & 0xFF;
    temp[1] = (new_png_IHDR_struct_data->width >> 16) & 0xFF;
    temp[2] = (new_png_IHDR_struct_data->width >> 8) & 0xFF;
    temp[3] = new_png_IHDR_struct_data->width & 0xFF;

    temp[4] = (new_png_IHDR_struct_data->height >> 24) & 0xFF;
    temp[5] = (new_png_IHDR_struct_data->height >> 16) & 0xFF;
    temp[6] = (new_png_IHDR_struct_data->height >> 8) & 0xFF;
    temp[7] = new_png_IHDR_struct_data->height & 0xFF;

    temp[8] = new_png_IHDR_struct_data->bit_depth;
    temp[9] = new_png_IHDR_struct_data->color_type;
    temp[10] = new_png_IHDR_struct_data->compression;
    temp[11] = new_png_IHDR_struct_data->filter;
    temp[12] = new_png_IHDR_struct_data->interlace;

    new_png->p_IHDR->length = 13;
    new_png->p_IHDR->type[0] = 'I';
    new_png->p_IHDR->type[1] = 'H';
    new_png->p_IHDR->type[2] = 'D';
    new_png->p_IHDR->type[3] = 'R';
    new_png->p_IHDR->p_data = temp;
    new_png->p_IHDR->crc = calculate_crc_value(new_png->p_IHDR);

    new_png->p_IDAT->length = IDAT_concatednated_compressed_length;
    new_png->p_IDAT->type[0] = 'I';
    new_png->p_IDAT->type[1] = 'D';
    new_png->p_IDAT->type[2] = 'A';
    new_png->p_IDAT->type[3] = 'T';
    new_png->p_IDAT->p_data = IDAT_concatenated_compressed;
    new_png->p_IDAT->crc = calculate_crc_value(new_png->p_IDAT);

    new_png->p_IEND->length = 0;
    new_png->p_IEND->type[0] = 'I';
    new_png->p_IEND->type[1] = 'E';
    new_png->p_IEND->type[2] = 'N';
    new_png->p_IEND->type[3] = 'D';
    new_png->p_IEND->crc = calculate_crc_value(new_png->p_IEND);

    FILE *fs = fopen("all.png", "w");
    fwrite(png_byte_header, 1, PNG_SIG_SIZE, fs);

    U32 IHDR_length = htonl(new_png->p_IHDR->length);
    fwrite(&IHDR_length, 1, CHUNK_LEN_SIZE, fs);
    fwrite(new_png->p_IHDR->type, 1, CHUNK_TYPE_SIZE, fs);
    fwrite(new_png->p_IHDR->p_data, 1, new_png->p_IHDR->length, fs);
    U32 IHDR_crc = htonl(new_png->p_IHDR->crc);
    fwrite(&IHDR_crc, 1, CHUNK_CRC_SIZE, fs);

    /* IDAT Chunk */
    U32 IDAT_length = htonl(new_png->p_IDAT->length);
    fwrite(&IDAT_length, 1, CHUNK_LEN_SIZE, fs);
    fwrite(new_png->p_IDAT->type, 1, CHUNK_TYPE_SIZE, fs);
    fwrite(new_png->p_IDAT->p_data, 1, new_png->p_IDAT->length, fs);
    U32 IDAT_crc = htonl(new_png->p_IDAT->crc);
    fwrite(&IDAT_crc, 1, CHUNK_CRC_SIZE, fs);

    /* IEND Chunk */
    U32 IEND_length = htonl(new_png->p_IEND->length);
    fwrite(&IEND_length, 1, CHUNK_LEN_SIZE, fs);
    fwrite(new_png->p_IEND->type, 1, CHUNK_TYPE_SIZE, fs);
    //fwrite(new_png->p_IDAT->p_data, 1, new_png->p_IDAT->length, fs);
    U32 IEND_crc = htonl(new_png->p_IEND->crc);
    fwrite(&IEND_crc, 1, CHUNK_CRC_SIZE, fs);

    free(concatenated_png_images);
    fclose(fs);

    return 0;
}
