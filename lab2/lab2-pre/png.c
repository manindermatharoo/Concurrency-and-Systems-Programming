#include "png.h"
#include <stdlib.h>
#include <string.h>
#include "arpa/inet.h" /* for htonl()                 */
#include "zutil.h"
#include "crc.h"

simple_PNG_p get_png_p() {

    simple_PNG_p png_img_p;
    png_img_p = malloc(sizeof(struct simple_PNG));
    png_img_p->p_IHDR = malloc(sizeof(struct chunk));
    png_img_p->p_IDAT = malloc(sizeof(struct chunk));
    png_img_p->p_IEND = malloc(sizeof(struct chunk));

    png_img_p->p_IHDR->length = 0;
    png_img_p->p_IDAT->length = 0;
    png_img_p->p_IEND->length = 0;

    return png_img_p;

}

int parse_png(simple_PNG_p png_p, char* png_raw) {

    png_p->p_IHDR = (chunk_p) malloc(sizeof(struct chunk));
    png_p->p_IDAT = (chunk_p) malloc(sizeof(struct chunk));
    png_p->p_IEND = (chunk_p) malloc(sizeof(struct chunk));

    int sig_size = PNG_SIG_SIZE;
    int chunk_size = CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE + CHUNK_CRC_SIZE;

    /* Parse IHDR */

    memcpy(&png_p->p_IHDR->length, png_raw + PNG_SIG_SIZE, CHUNK_LEN_SIZE);
    png_p->p_IHDR->length = ntohl(png_p->p_IHDR->length);

    memcpy(png_p->p_IHDR->type, png_raw + PNG_SIG_SIZE +CHUNK_LEN_SIZE , CHUNK_TYPE_SIZE);

    png_p->p_IHDR->p_data = (U8 *) malloc(DATA_IHDR_SIZE);

    memcpy(png_p->p_IHDR->p_data,
           png_raw + PNG_SIG_SIZE +CHUNK_LEN_SIZE +CHUNK_TYPE_SIZE,
           DATA_IHDR_SIZE);

    memcpy(&png_p->p_IHDR->crc,
           png_raw + PNG_SIG_SIZE +CHUNK_LEN_SIZE +CHUNK_TYPE_SIZE + DATA_IHDR_SIZE,
           CHUNK_CRC_SIZE);
    png_p->p_IHDR->crc = ntohl(png_p->p_IHDR->crc);


    int ihdr_chunk_size = chunk_size + DATA_IHDR_SIZE;

    /* Parse IDAT */

    memcpy(&png_p->p_IDAT->length, png_raw + sig_size + ihdr_chunk_size, CHUNK_LEN_SIZE);
    png_p->p_IDAT->length = ntohl(png_p->p_IDAT->length);

    memcpy(png_p->p_IDAT->type, png_raw + sig_size + ihdr_chunk_size +CHUNK_LEN_SIZE , CHUNK_TYPE_SIZE);

    png_p->p_IDAT->p_data = (U8 *)malloc(png_p->p_IDAT->length * sizeof(U8));

    memcpy(png_p->p_IDAT->p_data,
           png_raw + sig_size + ihdr_chunk_size + CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE,
           png_p->p_IDAT->length);

    memcpy(&png_p->p_IDAT->crc,
           png_raw + sig_size + ihdr_chunk_size + CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE + png_p->p_IDAT->length,
           CHUNK_CRC_SIZE);
    png_p->p_IDAT->crc = ntohl(png_p->p_IDAT->crc);

    int idat_chunk_size = chunk_size + png_p->p_IDAT->length;
    int total_sofar = idat_chunk_size + ihdr_chunk_size;

    /* Parse IEND */

    memcpy(&png_p->p_IEND->length, png_raw + sig_size + total_sofar, CHUNK_LEN_SIZE);

    memcpy(png_p->p_IEND->type, png_raw + sig_size + total_sofar + CHUNK_LEN_SIZE , CHUNK_TYPE_SIZE);

    png_p->p_IEND->p_data = NULL;

    memcpy(&png_p->p_IEND->crc,
           png_raw + sig_size + total_sofar + CHUNK_LEN_SIZE + CHUNK_TYPE_SIZE,
           CHUNK_CRC_SIZE);
    png_p->p_IEND->crc = ntohl(png_p->p_IDAT->crc);

    return 0;
}

simple_PNG_p concat_pngs(int num_pngs, simple_PNG_p* pngs) {

    image_data_p *images_p = (image_data_p *) malloc(num_pngs * sizeof(image_data_p));
    U32 height = 0;
    U32 width;
    U64 concat_size = 0;
    U64 concat_size_comp = 0;
    U8* concat_buf_comp;  /*Create buffer to hold IDAT data*/
    U8* concat_buf;  /*Create buffer to hold IDAT data*/

    chunk_p new_IHDR = malloc(sizeof(struct chunk));
    chunk_p new_IDAT = malloc(sizeof(struct chunk));
    chunk_p new_IEND = malloc(sizeof(struct chunk));

    for(int i = 0; i < num_pngs; i++) {

        simple_PNG_p png_p;             /*Pointer to png struct*/
        data_IHDR_p IHDR_struct_data ;  /*Pointer to IHDR struct data*/
        U8 *png_inf;                    /*Stores pixel data*/
        U64 length_inf;                 /*Length of decompressed pixel data*/
        image_data_p image_data;        /*Uncompressed image data*/

        png_p = pngs[i];
        IHDR_struct_data = get_IHDR_data(png_p);

        if(i == 0){
            /*Initialize IHDR based on new values*/
            *new_IHDR = *png_p->p_IHDR;

            /*Set width of final image based on new image*/
            width = get_png_width(IHDR_struct_data);

            *new_IDAT = *png_p->p_IDAT;

            *new_IEND = *png_p->p_IEND;
            compute_crc(new_IEND);
        }

        length_inf = (1 + 4 * width) *
            (get_png_height(IHDR_struct_data));

        png_inf = (U8 *) malloc(sizeof(U8) * length_inf);

        int inf_ret = mem_inf(png_inf,
                          &length_inf,
                          png_p->p_IDAT->p_data,
                          png_p->p_IDAT->length);

        if(inf_ret != 0){
            return NULL;
        }

        image_data = (image_data_p) malloc(sizeof(image_data_p));
        image_data->idat_uncompressed_data = png_inf;
        image_data->size = length_inf;
        image_data->width = htonl(get_png_width(IHDR_struct_data));
        image_data->height = htonl(get_png_height(IHDR_struct_data));

        images_p[i] = image_data;
        height += ntohl(image_data->height);

        concat_size += image_data->size;

        png_p = NULL;
    }

    concat_buf = (U8 *) malloc(concat_size);

    /* Concat all idata fields */

    U32 offset = 0;
    for(int i = 0; i < num_pngs; i++) {
        memcpy(concat_buf + offset, images_p[i]->idat_uncompressed_data, images_p[i]->size);
        offset += images_p[i]->size;
    }

    /*Compress concatenated data*/
    concat_buf_comp = (U8 *) malloc(2 * concat_size);
    mem_def(concat_buf_comp,
            &concat_size_comp,
            concat_buf,
            concat_size,
            Z_DEFAULT_COMPRESSION);

    /* Update height of image in IHDR*/
    set_png_height(new_IHDR, height);
    compute_crc(new_IHDR);

    /* Construct new IDAT */
    new_IDAT->length= (U32) concat_size_comp;
    new_IDAT->p_data = concat_buf_comp;
    /* *new_IDAT->type = *type_idat; */
    compute_crc(new_IDAT);

    /*Create png pointer and write to file*/
    simple_PNG_p png_concat = malloc(sizeof(struct simple_PNG));
    png_concat->p_IHDR = new_IHDR;
    png_concat->p_IDAT = new_IDAT;
    png_concat->p_IEND = new_IEND;

    return png_concat;

}

data_IHDR_p get_IHDR_data(simple_PNG_p png_p) {
    data_IHDR_p IHDR_struct_data = (data_IHDR_p)malloc(sizeof(DATA_IHDR_SIZE));
    get_png_data_IHDR(png_p->p_IHDR, IHDR_struct_data);
    return IHDR_struct_data;
}

int get_png_data_IHDR(struct chunk *info, struct data_IHDR *out) {
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

void compute_crc(chunk_p chunk){
    int buf_size = chunk->length + CHUNK_TYPE_SIZE;
    U8 buf[buf_size];

    for(int i = 0; i < CHUNK_TYPE_SIZE; i++)
    {
        buf[i] = chunk->type[i];
    }

    for(int i = 0; i < chunk->length; i++)
    {
        buf[CHUNK_TYPE_SIZE+i] = chunk->p_data[i];
    }

    U32 calculated_CRC = crc(buf, buf_size);
    chunk->crc = calculated_CRC;
}

int write_png_file(simple_PNG_p png_p, char* file_name){

    U8 png_byte_header[PNG_SIG_SIZE];
    png_byte_header[0] = 137; //89
    png_byte_header[1] = 80;  //50
    png_byte_header[2] = 78;  //4E
    png_byte_header[3] = 71;  //47
    png_byte_header[4] = 13;  //0D
    png_byte_header[5] = 10;  //0A
    png_byte_header[6] = 26;  //1A
    png_byte_header[7] = 10;  //0A

    FILE *png_file = fopen(file_name, "wb");
    fwrite(png_byte_header, 1, PNG_SIG_SIZE, png_file);
    write_png_chunk(png_p->p_IHDR, png_file);
    write_png_chunk(png_p->p_IDAT, png_file);
    write_png_chunk(png_p->p_IEND, png_file);

    fclose(png_file);

    return 0;
}

void set_png_height(chunk_p chunk, U32 height){
    U8 h_height[4];
    memcpy(h_height, (U8 *)&height, 4);
    chunk->p_data[7] = h_height[0];
    chunk->p_data[6] = h_height[1];
    chunk->p_data[5] = h_height[2];
    chunk->p_data[4] = h_height[3];
}

int write_png_chunk(chunk_p chunk, FILE* fp){
    U32 length_n = htonl(chunk->length);
    U32 crc_n = htonl(chunk->crc);

    fwrite(&length_n, 1, CHUNK_LEN_SIZE, fp);
    fwrite(chunk->type, 1, CHUNK_TYPE_SIZE, fp);
    if(chunk->length != 0){
        fwrite(chunk->p_data, 1, chunk->length, fp);
    }

    fwrite(&crc_n, 1, CHUNK_CRC_SIZE, fp);

    return 0;
}

/*
 * Return 0 if the pngs are the same
 * 1 if they are different
 */

int compare_png(simple_PNG_p png1, simple_PNG_p png2) {

    /* Compare lengths */

    if (png1->p_IDAT->length != png2->p_IDAT->length){
        return 1;
    }

    /* Compare IDAT */

    for(int i = 0; i < png1->p_IHDR->length; i++) {
        if (png1->p_IDAT->p_data[i] != png2->p_IDAT->p_data[i]) {
            return 1;
        }
    }

    /* Return a value of 0 if the pngs are the same */
    return 0;
}

int is_png(U8 *buf, size_t n)
{
    int file_is_png = 0;

    /* make sure that the file header was at least 8 bytes */
    if(n < 8)
    {
        file_is_png = 1;
    }
    else
    {
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
                break;
            }
        }
    }

    return file_is_png;
}
