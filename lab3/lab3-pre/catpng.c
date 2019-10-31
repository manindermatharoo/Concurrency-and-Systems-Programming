#include "catpng.h"

int uncompress_IDAT_image_data(struct PNG_file_data *compressed_png_file,
                               U8 *compressed_png_IDAT)
{
    int ret = 0;

    /* Calculate the length of the IDAT data after uncompression given by (1+4W)H */
    U64 IDAT_uncompressed_length = (1 + (4*get_png_width(compressed_png_file->IHDR_struct_data)))*(get_png_height(compressed_png_file->IHDR_struct_data));

    /* Uncompress IDAT data */
    ret = mem_inf(compressed_png_IDAT, &IDAT_uncompressed_length, \
                  compressed_png_file->png_format->p_IDAT->p_data, compressed_png_file->png_format->p_IDAT->length);

    return ret;
}

int png_files_same_width(struct PNG_file_data *png_file,
                         int total_number_of_images)
{
    int ret = 0;

    for(int i = 1; i < total_number_of_images; i++)
    {
        if((get_png_width(png_file[i].IHDR_struct_data)) != (get_png_width(png_file[0].IHDR_struct_data)))
        {
            ret = 1;
        }
    }

    return ret;
}

void calculate_concatenated_length_and_height(U64 *concatenated_png_length,
                                              U32 *total_concatenated_height,
                                              int total_number_of_images)
{
    for(int i = 0; i < total_number_of_images; i++)
    {
        *concatenated_png_length += IMAGE_SIZE;
        *total_concatenated_height += IMAGE_SEGMENT_HEIGHT;
    }
}

U8 * concatenate_uncompressed_png_images(struct IDAT_uncompressed_data *uncompressed_png_info,
                                         U64 concatenate_length,
                                         int total_number_of_images)
{
    U8 *buf = (U8 *)malloc(concatenate_length * sizeof(U8));
    U64 bytes_offset = 0;
    for(int i = 0; i < total_number_of_images; i++)
    {
        memcpy(buf + bytes_offset, uncompressed_png_info[25].IDAT_uncompressed_data, uncompressed_png_info[i].IDAT_uncompressed_length);
        bytes_offset += uncompressed_png_info[i].IDAT_uncompressed_length;
    }
    return buf;
}

U8 * concatenate_compressed_IDAT(U64 *compressed_data_length,
                                U8 *uncompressed_data,
                                U64 uncompressed_data_length)
{
    U8 *buf = (U8 *)malloc(2 * uncompressed_data_length * sizeof(U8));
    int ret = 0;
    ret = mem_def(buf, compressed_data_length, uncompressed_data, uncompressed_data_length, Z_DEFAULT_COMPRESSION);
    if (ret != 0)
    {
        fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
        free(buf);
        buf = NULL;
    }

    return buf;

}

int populate_IHDR_png_chunk(struct PNG_file_data *png_file,
                            U8 *IHDR_buf,
                            U32 image_height)
{
    /* Populate the IHDR length */
    png_file->png_format->p_IHDR->length = DATA_IHDR_SIZE;

    /* Populate the IHDR type */
    png_file->png_format->p_IHDR->type[0] = 'I';
    png_file->png_format->p_IHDR->type[1] = 'H';
    png_file->png_format->p_IHDR->type[2] = 'D';
    png_file->png_format->p_IHDR->type[3] = 'R';

    /* Populate the IHDR p_data, only change height keep all the original the same */
    IHDR_buf[0] = (IMAGE_SEGMENT_WIDTH >> 24) & 0xFF;
    IHDR_buf[1] = (IMAGE_SEGMENT_WIDTH >> 16) & 0xFF;
    IHDR_buf[2] = (IMAGE_SEGMENT_WIDTH >> 8) & 0xFF;
    IHDR_buf[3] = IMAGE_SEGMENT_WIDTH & 0xFF;

    IHDR_buf[4] = (image_height >> 24) & 0xFF;
    IHDR_buf[5] = (image_height >> 16) & 0xFF;
    IHDR_buf[6] = (image_height >> 8) & 0xFF;
    IHDR_buf[7] = image_height & 0xFF;

    IHDR_buf[8] = 8;
    IHDR_buf[9] = 6;
    IHDR_buf[10] = 0;
    IHDR_buf[11] = 0;
    IHDR_buf[12] = 0;

    png_file->png_format->p_IHDR->p_data = IHDR_buf;

    /* Populate the IHDR crc */
    png_file->png_format->p_IHDR->crc = calculate_crc_value(png_file->png_format->p_IHDR);

    return 0;
}

int populate_IDAT_png_chunk(struct chunk *out,
                            U8 *IDAT_compressed_data,
                            U64 IDAT_length)
{
    /* Populate the IDAT length */
    out->length = IDAT_length;

    /* Populate the IDAT type */
    out->type[0] = 'I';
    out->type[1] = 'D';
    out->type[2] = 'A';
    out->type[3] = 'T';

    /* Populate the IDAT p_data */
    out->p_data = IDAT_compressed_data;

    /* Populate the IDAT crc */
    out->crc = calculate_crc_value(out);

    return 0;
}

int populate_IEND_png_chunk(struct chunk *out)
{
    /* Populate the IDAT length */
    out->length = DATA_IEND_SIZE;

    /* Populate the IDAT type */
    out->type[0] = 'I';
    out->type[1] = 'E';
    out->type[2] = 'N';
    out->type[3] = 'D';

    /* Populate the IDAT crc */
    out->crc = calculate_crc_value(out);

    return 0;
}

int write_png_chunk(struct chunk *out,
                    FILE *fs)
{
     /* Write the chunk data length */
    U32 length = htonl(out->length);
    fwrite(&length, 1, CHUNK_LEN_SIZE, fs);

    /* Write the chunk type */
    fwrite(out->type, 1, CHUNK_TYPE_SIZE, fs);

    /* Write the chunk data */
    if(out->length != 0)
    {
        fwrite(out->p_data, 1, out->length, fs);
    }
    else
    {
        out->p_data = NULL;
    }

    /* Write the chunk CRC */
    U32 calculated_crc = htonl(out->crc);
    fwrite(&calculated_crc, 1, CHUNK_CRC_SIZE, fs);

    return 0;
}

int create_new_png(struct PNG_file_data *new_png_file,
                   U8 *concated_IDAT_data,
                   U64 new_png_file_length,
                   U32 new_png_height,
                   char *file_name)
{
    /* Open binary png file */
    FILE *save_png_file = fopen(file_name, "w");

    /* make sure the file is a valid file to open (exists) */
    if(save_png_file == NULL)
    {
        fprintf(stderr, "Couldn't create %s: %s\n", file_name, strerror(errno));
        exit(1);
    }

    /* Create all variables */
    initialize_PNG_file_struct(new_png_file);

    U8 *IHDR_data = (U8 *)malloc(sizeof(U8) * DATA_IHDR_SIZE);

    /* Write png header */
    char png_byte_header[PNG_SIG_SIZE];
    png_byte_header[0] = 137; //89
    png_byte_header[1] = 80;  //50
    png_byte_header[2] = 78;  //4E
    png_byte_header[3] = 71;  //47
    png_byte_header[4] = 13;  //0D
    png_byte_header[5] = 10;  //0A
    png_byte_header[6] = 26;  //1A
    png_byte_header[7] = 10;  //0A

    memcpy(new_png_file->png_file_header, png_byte_header, PNG_SIG_SIZE);

    fwrite(new_png_file->png_file_header, 1, PNG_SIG_SIZE, save_png_file);

    populate_IHDR_png_chunk(new_png_file, IHDR_data, new_png_height);
    populate_IDAT_png_chunk(new_png_file->png_format->p_IDAT, concated_IDAT_data, new_png_file_length);
    populate_IEND_png_chunk(new_png_file->png_format->p_IEND);

    write_png_chunk(new_png_file->png_format->p_IHDR, save_png_file);
    write_png_chunk(new_png_file->png_format->p_IDAT, save_png_file);
    write_png_chunk(new_png_file->png_format->p_IEND, save_png_file);

    fclose(save_png_file);

    return 0;
}

int concatenate_png_chunks(U8 *png_chunks_IDAT_data,
                           int image_count)
{
    /* Create all variables */
    // int png_files_are_good = 0;
    // int png_files_have_same_width = 0;
    // int IDAT_uncompression_successful = 0;

    int total_number_of_images = image_count;

    // PNG_file_data_p png_images = (PNG_file_data_p)malloc(total_number_of_images * sizeof(struct PNG_file_data));
    IDAT_uncompressed_data_p uncompressed_data_png_images = (IDAT_uncompressed_data_p)malloc(total_number_of_images * sizeof(struct IDAT_uncompressed_data));

    U64 IDAT_concatenated_uncompressed_length = 0;
    U8 *IDAT_concatenated_uncompressed = NULL;
    U64 IDAT_concatenated_compressed_length = 0;
    U8 *IDAT_concatenated_compressed = NULL;

    U32 concatenated_png_height = 0;

    char *concatenated_file_name = "all.png";
    PNG_file_data_p concatenated_png = (PNG_file_data_p)malloc(sizeof(struct PNG_file_data));

    /* Loop through all png images and uncompress IDAT data */
    // for(int i = 0; i < image_count; i++)
    // {
        // /* Process the png file and make sure everything is valid */
        // png_files_are_good = process_png_file(&png_images[i], png_chunks[i].buf, png_chunks[i].size);
        // if(png_files_are_good != 0)
        // {
        //     printf("PNG file is not good %d.\n", png_chunks[i].seq);
        //     exit(0);
        // }

        // /* Uncompress IDAT data for each png image */
        // IDAT_uncompression_successful = uncompress_IDAT_image_data(&png_images[i], &uncompressed_data_png_images[i]);
        // if(IDAT_uncompression_successful != 0)
        // {
        //     printf("IDAT mem_inf uncompression not succesful.\n");
        //     exit(0);
        // }
    // }

    /* Ensure the images have the same width */
    // png_files_have_same_width = png_files_same_width(png_images, total_number_of_images);
    // if(png_files_have_same_width != 0)
    // {
    //     printf("The PNG files do not have the same width.\n");
    //     exit(0);
    // }

    /* Calculate the total length and height for concatenated uncompressed image */
    calculate_concatenated_length_and_height(&IDAT_concatenated_uncompressed_length,
                                             &concatenated_png_height,
                                             total_number_of_images);

    /* Free up all the memory for png files except the first one so it can be used to create new IDAT */
    // for(int i = 1; i < image_count; i++)
    // {
    //     free_PNG_file_struct(&png_images[i]);
    // }

    /* Concatenate the uncompressed images together */
    // IDAT_concatenated_uncompressed = concatenate_uncompressed_png_images(png_chunks_IDAT_data->items,
    //                                                                      IDAT_concatenated_uncompressed_length,
    //                                                                      total_number_of_images);

    /* Create a concatenated compressed image */
    IDAT_concatenated_compressed = concatenate_compressed_IDAT(&IDAT_concatenated_compressed_length,
                                                               png_chunks_IDAT_data,
                                                               IDAT_concatenated_uncompressed_length);

    /* IDAT concatenated compression not successful */
    if(IDAT_concatenated_compressed == NULL)
    {
        printf("IDAT mem_def uncompression not succesful.\n");
        exit(0);
    }

    /* Create a new image */
    create_new_png(concatenated_png,
                   IDAT_concatenated_compressed,
                   IDAT_concatenated_compressed_length,
                   concatenated_png_height,
                   concatenated_file_name);

    /* Deallocate used memory */
    free_PNG_file_struct(concatenated_png);
    free(concatenated_png);

    free(uncompressed_data_png_images);

    free(IDAT_concatenated_uncompressed);

    return 0;
}
