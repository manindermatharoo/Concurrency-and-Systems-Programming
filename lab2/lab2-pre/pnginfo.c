#include <errno.h>     /* for errno                   */
#include <stdio.h>     /* for printf(), perror()...   */
#include <stdlib.h>    /* for malloc()                */
#include <string.h>    /* for strstr()                */
#include "arpa/inet.h" /* for htonl()                 */
#include "crc.h"       /* for crc()                   */
#include "lab_png.h"   /* simple PNG data structures  */

int is_png(char *buf)
{
    int file_is_png = 0;

    /* Create array that matches header of a png file */
    char png_byte_header[PNG_SIG_SIZE];
    png_byte_header[0] = 137; //89
    png_byte_header[1] = 80;  //50
    png_byte_header[2] = 78;  //4E
    png_byte_header[3] = 71;  //47
    png_byte_header[4] = 13;  //0D
    png_byte_header[5] = 10;  //0A
    png_byte_header[6] = 26;  //1A
    png_byte_header[7] = 10;  //0A

    /* Make sure the header of the file matches a png header */
    for(int i = 0; i < PNG_SIG_SIZE; i++)
    {
        if(buf[i] != png_byte_header[i])
        {
            file_is_png = 1;
            break;
        }
    }

    return file_is_png;
}

int process_png_chunk(struct chunk *out, U8 *data, char *buf, int *buf_offset)
{
    /* Get the chunk data length */
    memcpy(&out->length, buf  + *buf_offset, CHUNK_LEN_SIZE);
    *buf_offset += CHUNK_LEN_SIZE;
    out->length = ntohl(out->length);

    /* Get the chunk type */
    memcpy(out->type, buf  + *buf_offset, CHUNK_TYPE_SIZE);
    *buf_offset += CHUNK_LEN_SIZE;

    /* Get the chunk data */
    if(out->length != 0)
    {
        data = (U8 *)malloc(out->length * sizeof(U8));
        memcpy(data, buf  + *buf_offset, out->length);
        *buf_offset += out->length;
        out->p_data = data;
    }
    else
    {
        out->p_data = NULL;
    }

    /* Get the chunk CRC */
    memcpy(&out->crc, buf  + *buf_offset, CHUNK_CRC_SIZE);
    *buf_offset += CHUNK_CRC_SIZE;
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
    int ret = 0;

    /* Calculate CRC */
    U32 calculated_CRC = calculate_crc_value(out);

    /* Compare the calculated and expected CRC values */
    if(out->crc != calculated_CRC)
    {
        for(int i = 0; i < CHUNK_TYPE_SIZE; i++)
        {
            printf("%c", out->type[i]);
        }
        printf(" chunk CRC error: computed %x, expected %x \n", calculated_CRC, out->crc);
        ret = 1;
    }

    return ret;
}

U32 calculate_crc_value(struct chunk *out)
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

    return calculated_CRC;
}

void initialize_PNG_file_struct(struct PNG_file_data *png_image)
{
    png_image->png_file_header = (char *)malloc(PNG_SIG_SIZE * sizeof(char));

    png_image->png_format = (simple_PNG_p)malloc(sizeof(struct simple_PNG));
    png_image->png_format->p_IHDR = (chunk_p)malloc(sizeof(struct chunk));
    png_image->png_format->p_IDAT = (chunk_p)malloc(sizeof(struct chunk));
    png_image->png_format->p_IEND = (chunk_p)malloc(sizeof(struct chunk));

    png_image->p_IHDR_data = NULL;
    png_image->p_IDAT_data = NULL;
    png_image->p_IEND_data = NULL;

    png_image->IHDR_struct_data = (data_IHDR_p)malloc(sizeof(DATA_IHDR_SIZE));
}

int process_png_file(struct PNG_file_data *png_image, char *raw_png, int raw_png_size)
{
    int ret = 0;

    int png_offset = 0; /* Update the offset from where we want to read the raw_png bytes */

    /* Make sure there is enough bytes to read to check is_png */
    if(raw_png_size < 8)
    {
        ret = 1;
        return ret;
    }

    /* Create all variables */
    initialize_PNG_file_struct(png_image);

    int ret_IHDR_CRC_check = 0;
    int ret_IDAT_CRC_check = 0;
    int ret_IEND_CRC_check = 0;


    /* Read the first 8 bytes of png file which should be the header */
    memcpy(png_image->png_file_header, raw_png  + png_offset, PNG_SIG_SIZE);
    png_offset += PNG_SIG_SIZE;

    /* Make sure the file is a png before preceeding */
    if(is_png(png_image->png_file_header) != 0)
    {
        printf("Not a PNG file\n");
        ret = 1;
    }
    else
    {
        /* Collect all the information from each chunk */
        process_png_chunk(png_image->png_format->p_IHDR, png_image->p_IHDR_data, raw_png, &png_offset);
        process_png_chunk(png_image->png_format->p_IDAT, png_image->p_IDAT_data, raw_png, &png_offset);
        process_png_chunk(png_image->png_format->p_IEND, png_image->p_IEND_data, raw_png, &png_offset);

        /* Fill out the IHDR data structure */
        get_png_data_IHDR(png_image->png_format->p_IHDR, png_image->IHDR_struct_data);

        /* Compute CRC checks */
        ret_IHDR_CRC_check = check_crc_value(png_image->png_format->p_IHDR);
        ret_IDAT_CRC_check = check_crc_value(png_image->png_format->p_IDAT);
        ret_IEND_CRC_check = check_crc_value(png_image->png_format->p_IEND);

        if((ret_IHDR_CRC_check != 0) || (ret_IDAT_CRC_check != 0) || (ret_IEND_CRC_check != 0))
        {
            ret = 1;
        }
    }

    return ret;
}
