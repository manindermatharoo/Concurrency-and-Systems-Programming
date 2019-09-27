#include <stdlib.h>
#include <stdio.h>
#include <errno.h>     /* for errno                   */
#include <string.h>
#include "zutil.h"
#include "arpa/inet.h" /* for htonl()                 */
#include "lab_png.h"
#include "crc.h"

int check_png(char* png_path);
int check_crc(simple_PNG_p png_p);
simple_PNG_p read_png(char* png_path);
data_IHDR_p get_IHDR_data(simple_PNG_p png_p);
void set_png_height(chunk_p chunk, U32 height);
void compute_crc(chunk_p chunk);
int write_png_file(simple_PNG_p png_p);
int write_png_chunk(chunk_p chunk, FILE* fp);

int main(int argc, char** argv){

    U8* concat_buf;  /*Create buffer to hold IDAT data*/
    U8* concat_buf_comp;  /*Create buffer to hold IDAT data*/
    U64 concat_size = 0;
    U64 concat_size_comp = 0;
    image_data_p *images_p = (image_data_p *) malloc(argc * sizeof(image_data_p));
    U32 width;
    U32 height = 0;

    chunk_p new_IHDR = malloc(sizeof(struct chunk));
    chunk_p new_IDAT = malloc(sizeof(struct chunk));
    chunk_p new_IEND = malloc(sizeof(struct chunk));

    for(int i=1; i<argc; i++){

        /* Create all variables */
        simple_PNG_p png_p;             /*Pointer to png struct*/
        data_IHDR_p IHDR_struct_data ;  /*Pointer to IHDR struct data*/
        U8 *png_inf;                    /*Stores pixel data*/
        U64 length_inf;                 /*Length of decompressed pixel data*/
        image_data_p image_data;        /*Uncompressed image data*/

        /*Check if valid png*/
        if(check_png(argv[i]) != 0){
            printf("%s is not a png file\n", argv[i]);
            return 1;
        }

        png_p = read_png(argv[i]);
        IHDR_struct_data = get_IHDR_data(png_p);

        if(check_crc(png_p) != 0){
            printf("CRC check failed for image %s\n", argv[1]);
            return 1;
        }

        if(i == 1){
            /*Initialize IHDR based on new values*/
            *new_IHDR = *png_p->p_IHDR;
            compute_crc(new_IHDR);

            /*Set width of final image based on new image*/
            width = get_png_width(IHDR_struct_data);
            *new_IDAT = *png_p->p_IDAT;
        }
        else{
            if(get_png_width(IHDR_struct_data) != width){
                printf("%s is not the same width as preceding files.\n", argv[i]);
                return 1;
            }
        }

        if(i == argc - 1){
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
            printf("Error code %d. Unable to decompress %s\n",
                   inf_ret, argv[i]);
            return 1;
        }

        image_data = (image_data_p) malloc(sizeof(image_data_p));

        image_data->idat_uncompressed_data = png_inf;
        image_data->size = length_inf;
        image_data->width = htonl(get_png_width(IHDR_struct_data));
        image_data->height = htonl(get_png_height(IHDR_struct_data));

        images_p[i-1] = image_data;
        height += ntohl(image_data->height);

        /*Add up the sizes*/
        concat_size += image_data->size;

        /*Free pointers*/
        free(png_p->p_IHDR);
        free(png_p->p_IDAT);
        free(png_p->p_IEND);
        free(png_p);
    }
    concat_buf = (U8 *) malloc(concat_size);

    /* Concat all idata fields */
   
    int images_index = 0;
    int total_size = 0;
    for(int i = 0; i < concat_size; i++){
        if(i == images_p[images_index]->size + total_size){
            images_index++;
            total_size += i;
        }
        /* if(images_index == 0){ */
        /*     concat_buf[i] = images_p[images_index]->idat_uncompressed_data[i]; */
        /* } */
        /* else{ */

            concat_buf[i] = images_p[images_index]->idat_uncompressed_data[i-total_size];
        /* } */
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

    write_png_file(png_concat);
    return 0;
}

int check_png(char *png_path){

    U8 png_file_header[PNG_SIG_SIZE];

    FILE *png_file = fopen(png_path, "rb");

    /* Read the first 8 bytes of png file which should be the header */
    int header_bytes = fread(png_file_header, 1, PNG_SIG_SIZE, png_file);
    int ret = is_png(png_file_header, header_bytes);
    fclose(png_file);
    return ret;
}

simple_PNG_p read_png(char* png_path){

    simple_PNG_p png_format = (simple_PNG_p) malloc(sizeof(struct simple_PNG));
    png_format->p_IHDR = (chunk_p) malloc(sizeof(struct chunk));
    png_format->p_IDAT = (chunk_p) malloc(sizeof(struct chunk));
    png_format->p_IEND = (chunk_p) malloc(sizeof(struct chunk));

    U8 *p_IHDR_data = NULL; /* Stores data from IHDR chunk */
    U8 *p_IDAT_data = NULL; /* Stores data from IDAT chunk */
    U8 *p_IEND_data = NULL; /* Stores data from IEND chunk */

    /* Open binary png file */
    FILE *png_file = fopen(png_path, "rb");

    /*Need to do this to skip the PNG signature*/
    fseek(png_file, PNG_SIG_SIZE, SEEK_CUR);

    process_png_chunk(png_format->p_IHDR, p_IHDR_data, png_file);
    process_png_chunk(png_format->p_IDAT, p_IDAT_data, png_file);
    process_png_chunk(png_format->p_IEND, p_IEND_data, png_file);

    fclose(png_file);
    return png_format;
}

data_IHDR_p get_IHDR_data(simple_PNG_p png_p){
    data_IHDR_p IHDR_struct_data = (data_IHDR_p)malloc(sizeof(DATA_IHDR_SIZE));
    get_png_data_IHDR(png_p->p_IHDR, IHDR_struct_data);
    return IHDR_struct_data;
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

void set_png_height(chunk_p chunk, U32 height){
    U8 h_height[4];
    memcpy(h_height, (U8 *)&height, 4);
    chunk->p_data[7] = h_height[0];
    chunk->p_data[6] = h_height[1];
    chunk->p_data[5] = h_height[2];
    chunk->p_data[4] = h_height[3];
}

int write_png_file(simple_PNG_p png_p){

    U8 png_byte_header[PNG_SIG_SIZE];
    png_byte_header[0] = 137; //89
    png_byte_header[1] = 80;  //50
    png_byte_header[2] = 78;  //4E
    png_byte_header[3] = 71;  //47
    png_byte_header[4] = 13;  //0D
    png_byte_header[5] = 10;  //0A
    png_byte_header[6] = 26;  //1A
    png_byte_header[7] = 10;  //0A

    FILE *png_file = fopen("./concat.png", "wb");
    fwrite(png_byte_header, 1, PNG_SIG_SIZE, png_file);
    write_png_chunk(png_p->p_IHDR, png_file);
    write_png_chunk(png_p->p_IDAT, png_file);
    write_png_chunk(png_p->p_IEND, png_file);

    fclose(png_file);

    return 0;
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

int check_crc(simple_PNG_p png_p){
    if(check_crc_value(png_p->p_IHDR) != 0 ||
       check_crc_value(png_p->p_IDAT) != 0 ||
       check_crc_value(png_p->p_IEND) != 0) return 1;

    return 0;
}
