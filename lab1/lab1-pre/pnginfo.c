#include <errno.h>    /* for errno                   */
#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include "arpa/inet.h" /* for htonl()                */
#include "crc.h"      /* for crc()                   */
#include "lab_png.h"  /* simple PNG data structures  */

int is_png(U8 *buf, size_t n)
{
    int file_is_png = 0;

    /* Create array that matches header of a png file */
    U8 png_byte_header[n];
    png_byte_header[0] = 137; //89
    png_byte_header[1] = 80;  //50
    png_byte_header[2] = 78;  //4E
    png_byte_header[3] = 71;  //47
    png_byte_header[4] = 13;  //0D
    png_byte_header[5] = 10;  //0A
    png_byte_header[6] = 26;  //1A
    png_byte_header[7] = 10;  //0A

    /* Make sure the header of the file matches a png header */
    for(int i = 0; i < n; i++)
    {
        if(buf[i] != png_byte_header[i])
        {
            file_is_png = 1;
        }
    }

    return file_is_png;
}

int process_png_chunk(struct chunk *out, U8 *data, FILE *fp)
{
    /* Get the chunk data length */
    fread(&out->length, 1, CHUNK_LEN_SIZE, fp);
    out->length = ntohl(out->length);

    /* Get the chunk type */
    fread(out->type, 1, CHUNK_TYPE_SIZE, fp);

    /* Get the chunk data */
    if(out->length != 0)
    {
        data = (U8 *)malloc(out->length * sizeof(U8));
        fread(data, 1, out->length, fp);
        out->p_data = data;
    }
    else
    {
        out->p_data = NULL;
    }

    /* Get the chunk CRC */
    fread(&out->crc, 1, CHUNK_CRC_SIZE, fp);
    out->crc = ntohl(out->crc);

    return 0;
}

int get_png_data_IHDR(struct chunk *info, struct data_IHDR *out)
{
    out->width = ntohl((info->p_data[3] << 24) + (info->p_data[2] << 16) + (info->p_data[1] << 8) + (info->p_data[0]));
    out->height = ntohl((info->p_data[7] << 24) + (info->p_data[6] << 16) + (info->p_data[5] << 8) + (info->p_data[4]));
    out->bit_depth = info->p_data[8];
    out->color_type = info->p_data[9];
    out->compression = info->p_data[10];
    out->filter = info->p_data[11];
    out->interlace = info->p_data[12];

    return 0;
}

U32 get_png_height(struct data_IHDR *buf)
{
    return buf->height;
}

U32 get_png_width(struct data_IHDR *buf)
{
    return buf->width;
}

int check_crc_value(struct chunk *out)
{
    /* Concatenate the type and data fields together */
    int buf_size = out->length + CHUNK_TYPE_SIZE;
    U8 buf[buf_size];

    for(int i = 0; i < CHUNK_TYPE_SIZE; i++)
    {
        buf[i] = out->type[i];
    }

    for(int i = 0; i < out->length; i++)
    {
        buf[CHUNK_TYPE_SIZE+i] = out->p_data[i];
    }

    /* Calculate CRC */
    U32 calculated_CRC = crc(buf, buf_size);

    /* Compare the calculated and expected CRC values */
    if(out->crc != calculated_CRC)
    {
        for(int i = 0; i < CHUNK_TYPE_SIZE; i++)
        {
            printf("%c", out->type[i]);
        }
        printf(" chunk CRC error: computed %x, expected %x \n", calculated_CRC, out->crc);
    }

    return 0;
}

int main (int argc, char **argv)
{
    /* Ensure there is one argument */
    if (argc == 1 || argc > 2) {
        fprintf(stderr, "Usage: %s <png file>\n", argv[0]);
        exit(1);
    }

    /* Create all variables */
    U8 png_file_header[PNG_SIG_SIZE];

    simple_PNG_p png_format = (simple_PNG_p)malloc(sizeof(struct simple_PNG));
    png_format->p_IHDR = (chunk_p)malloc(sizeof(struct chunk));
    png_format->p_IDAT = (chunk_p)malloc(sizeof(struct chunk));
    png_format->p_IEND = (chunk_p)malloc(sizeof(struct chunk));

    U8 *p_IHDR_data = NULL; /* Stores data from IHDR chunk */
    U8 *p_IDAT_data = NULL; /* Stores data from IDAT chunk */
    U8 *p_IEND_data = NULL; /* Stores data from IEND chunk */

    data_IHDR_p IHDR_struct_data = (data_IHDR_p)malloc(sizeof(DATA_IHDR_SIZE));

    /* Open binary png file */
    FILE *png_file = fopen(argv[1], "rb");

    /* Read the first 8 bytes of png file which should be the header */
    int header_bytes = fread(png_file_header, 1, PNG_SIG_SIZE, png_file);

    /* Make sure the file is a png before preceeding */
    if(is_png(png_file_header, header_bytes) != 0 || header_bytes < 8)
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
        get_png_data_IHDR(png_format->p_IHDR, IHDR_struct_data);

        printf("%s: %d x %d\n", argv[1], get_png_width(IHDR_struct_data), get_png_height(IHDR_struct_data));

        /* Compute CRC checks */
        check_crc_value(png_format->p_IHDR);
        check_crc_value(png_format->p_IDAT);
        check_crc_value(png_format->p_IEND);
    }

    /* Clear all dynamically allocated memory */
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

    return 0;
}