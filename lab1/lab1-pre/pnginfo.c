#include "lab_png.h"  /* simple PNG data structures  */
#include <stdio.h>    /* for printf(), perror()...   */
#include <stdlib.h>   /* for malloc()                */
#include <errno.h>    /* for errno                   */
#include "crc.h"      /* for crc()                   */
#include "arpa/inet.h" /* for htonl()                */

int is_png(U8 *buf, size_t n)
{
    int file_is_png = 0;

    U8 png_byte_header[n];
    png_byte_header[0] = 137; //89
    png_byte_header[1] = 80;  //50
    png_byte_header[2] = 78;  //4E
    png_byte_header[3] = 71;  //47
    png_byte_header[4] = 13;  //0D
    png_byte_header[5] = 10;  //0A
    png_byte_header[6] = 26;  //1A
    png_byte_header[7] = 10;  //0A

    for(int i = 0; i < n; i++)
    {
        if(buf[i] != png_byte_header[i])
        {
            file_is_png = 1;
            return file_is_png;
        }
    }
    return file_is_png;
}

int get_png_data_IHDR(U8* data, U32 data_length, struct data_IHDR *out, FILE *fp)
{
    fread(data, 1, data_length, fp);
    out->width = ntohl((data[3] << 24) + (data[2] << 16) + (data[1] << 8) + (data[0]));
    out->height = ntohl((data[7] << 24) + (data[6] << 16) + (data[5] << 8) + (data[4]));
    out->bit_depth = data[8];
    out->color_type = data[9];
    out->compression = data[10];
    out->filter = data[11];
    out->interlace = data[12];
    return 0;
}

int main (int argc, char **argv)
{
    int ret = 0;

    if (argc == 1 || argc > 2) {
        fprintf(stderr, "Usage: %s <png file>\n", argv[0]);
        exit(1);
    }

    FILE * input_file = fopen(argv[1], "rb");

    U8 png_header[PNG_SIG_SIZE];
    fread(png_header, 1, PNG_SIG_SIZE, input_file);
    
    ret = is_png(png_header, PNG_SIG_SIZE);
    
    if(ret != 0)
    {
        printf("%s: Not a PNG file\n", argv[1]);
        exit(1);
    }

    simple_PNG_p png_file;
    png_file = (simple_PNG_p)malloc(sizeof(struct simple_PNG));

    png_file->p_IHDR = (chunk_p)malloc(sizeof(struct chunk));
    
    /* get the IHDR length*/
    fread(&png_file->p_IHDR->length, 1, CHUNK_LEN_SIZE, input_file);
    png_file->p_IHDR->length = ntohl(png_file->p_IHDR->length);

    /* get the IHDR type*/
    fread(png_file->p_IHDR->type, 1, CHUNK_TYPE_SIZE, input_file);

    data_IHDR_p IHDR_data;
    IHDR_data = (data_IHDR_p)malloc(DATA_IHDR_SIZE);

    U8 all_IDHR_data[png_file->p_IHDR->length];

    get_png_data_IHDR(all_IDHR_data, png_file->p_IHDR->length, IHDR_data, input_file);
    png_file->p_IHDR->p_data = all_IDHR_data;
    printf("%s: %d x %d\n", argv[1], IHDR_data->width, IHDR_data->height);

    int buf_size = png_file->p_IHDR->length + CHUNK_TYPE_SIZE;
    U8 buf[buf_size];

    for(int i = 0; i < CHUNK_TYPE_SIZE; i++)
    {
        buf[i] = png_file->p_IHDR->type[i];
    }
    
    for(int i = 0; i < png_file->p_IHDR->length; i++)
    {
        buf[4+i] = all_IDHR_data[i];
    }

    U32 IHDR_crc = crc(buf, buf_size);

    fread(&png_file->p_IHDR->crc, 1, CHUNK_CRC_SIZE, input_file);
    png_file->p_IHDR->crc = ntohl(png_file->p_IHDR->crc);

    if(png_file->p_IHDR->crc != IHDR_crc)
    {
        printf("IHDR chunk CRC error: computed %x, expected %x \n", IHDR_crc, png_file->p_IHDR->crc);
    }

    png_file->p_IDAT = (chunk_p)malloc(sizeof(struct chunk));

    /* read the length of IDAT data */
    fread(&png_file->p_IDAT->length, 1, CHUNK_LEN_SIZE, input_file);
    png_file->p_IDAT->length = ntohl(png_file->p_IDAT->length);

    /* read the type of chunk */
    fread(png_file->p_IDAT->type, 1, CHUNK_TYPE_SIZE, input_file);

    U8 IDAT_data[png_file->p_IDAT->length];
    fread(IDAT_data, 1, png_file->p_IDAT->length, input_file);

    png_file->p_IDAT->p_data = IDAT_data;

    int buf_size_IDAT = png_file->p_IDAT->length + CHUNK_TYPE_SIZE;
    U8 IDAT_buf[buf_size_IDAT];

    for(int i = 0; i < CHUNK_TYPE_SIZE; i++)
    {
        IDAT_buf[i] = png_file->p_IDAT->type[i];
    }
    
    for(int i = 0; i < png_file->p_IDAT->length; i++)
    {
        IDAT_buf[4+i] = png_file->p_IDAT->p_data[i];
    }

    U32 IDAT_crc = crc(IDAT_buf, buf_size_IDAT);

    fread(&png_file->p_IDAT->crc, 1, CHUNK_CRC_SIZE, input_file);
    png_file->p_IDAT->crc = ntohl(png_file->p_IDAT->crc);

    if(png_file->p_IDAT->crc != IDAT_crc)
    {
        printf("IDAT chunk CRC error: computed %x, expected %x \n", IDAT_crc, png_file->p_IDAT->crc);
    }

    png_file->p_IEND = (chunk_p)malloc(sizeof(struct chunk));

    /* read the length of IDAT data */
    fread(&png_file->p_IEND->length, 1, CHUNK_LEN_SIZE, input_file);
    png_file->p_IEND->length = ntohl(png_file->p_IEND->length);

    /* read the type of chunk */
    fread(png_file->p_IEND->type, 1, CHUNK_TYPE_SIZE, input_file);

    U8 IEND_data[png_file->p_IEND->length];
    fread(IEND_data, 1, png_file->p_IEND->length, input_file);

    png_file->p_IEND->p_data = NULL;

    int buf_size_IEND = png_file->p_IEND->length + CHUNK_TYPE_SIZE;
    U8 IEND_buf[buf_size_IEND];

    for(int i = 0; i < CHUNK_TYPE_SIZE; i++)
    {
        IEND_buf[i] = png_file->p_IEND->type[i];
    }
    
    for(int i = 0; i < png_file->p_IEND->length; i++)
    {
        IEND_buf[4+i] = png_file->p_IEND->p_data[i];
    }

    U32 IEND_crc = crc(IEND_buf, buf_size_IEND);

    fread(&png_file->p_IEND->crc, 1, CHUNK_CRC_SIZE, input_file);
    png_file->p_IEND->crc = ntohl(png_file->p_IEND->crc);

    if(png_file->p_IEND->crc != IEND_crc)
    {
        printf("IEND chunk CRC error: computed %x, expected %x \n", IEND_crc, png_file->p_IEND->crc);
    }

    free(png_file->p_IEND);
    free(png_file->p_IHDR);
    free(png_file->p_IDAT);
    free(png_file);

    fclose(input_file);

    return 0;
}