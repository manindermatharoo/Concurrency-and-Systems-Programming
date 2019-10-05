#include "paster.h"

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}

size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;

    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	return 2;
    }

    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be non-negative */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
	return 1;
    }

    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}

int command_line_options(int *argt, int *argn, int argc, char ** argv)
{
    int option;
    char *str = "option requires an argument";

    while ((option = getopt(argc, argv, "t:n:")) != -1)
    {
        switch (option)
        {
            case 't':
                *argt = strtoul(optarg, NULL, 10);
                if (*argt <= 0)
                {
                    fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                    return -1;
                }
#ifdef DEBUG_1
                printf("Number of threads selected are %d.\n", *argt);
#endif
                break;
            case 'n':
                *argn = strtoul(optarg, NULL, 10);
                if (*argn <= 0 || *argn > 3)
                {
                    fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
                    return -1;
                }
#ifdef DEBUG_1
                printf("Image chosen is %d.\n", *argn);
#endif
                break;
            default:
                return -1;
        }
    }

    return 0;
}

void *send_curl(void *arg)
{
    /* Use current time as seed for random generator */
    srand(time(0));

    int IDAT_uncompression_successful = 0;
    int png_files_are_good = 0;

    struct recv_multiple_chunks *all_png_chunks = (struct recv_multiple_chunks *)arg;

    /* Initialize and setup cURL */
    CURL *curl_handle;
    CURLcode res;
    RECV_BUF recv_buf;

    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return NULL;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, all_png_chunks->url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* Loop until all png chunks have been stored */
    while(all_png_chunks->number_of_images_received < TOTAL_PNG_CHUNKS)
    {
        /* Randomly pick a server between 1 and 3 to fetch image */
        int upper = 3;
        int lower = 1;
        int int_num = (rand() % (upper - lower + 1)) + lower;
        char num = (char)( ((int) '0') + int_num);
        all_png_chunks->url[14] = num;

        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, all_png_chunks->url);

        recv_buf_init(&recv_buf, BUF_SIZE);

        /* get it! */
        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        if(all_png_chunks->checked_png_chunk[recv_buf.seq] == false)
        {
            png_files_are_good = process_png_file(&all_png_chunks->png_img_chunk[recv_buf.seq], recv_buf.buf, recv_buf.size);
            if(png_files_are_good != 0)
            {
                printf("PNG file is not good %d.\n", recv_buf.seq);
                exit(0);
            }

            /* Uncompress IDAT data for each png image */
            IDAT_uncompression_successful = uncompress_IDAT_image_data(&all_png_chunks->png_img_chunk[recv_buf.seq], &all_png_chunks->uncompressed_data_png_images[recv_buf.seq]);
            if(IDAT_uncompression_successful != 0)
            {
                printf("IDAT mem_inf uncompression not succesful.\n");
                exit(0);
            }

            all_png_chunks->number_of_images_received++;

#ifdef DEBUG_1
            printf("%lu bytes received in memory %p, seq=%d.\n", \
                recv_buf.size, recv_buf.buf, recv_buf.seq);
#endif

            all_png_chunks->checked_png_chunk[recv_buf.seq] = true;
        }
        recv_buf_cleanup(&recv_buf);
    }

    /* cleaning up */
    curl_easy_cleanup(curl_handle);

    return NULL;
}

int main( int argc, char** argv )
{
    int num_threads = 1; /* number of threads being used; for single threaded deafult to 1 */
    int img_number = 1; /* select the web sever; defaults to webserver 1 */

    /* Check if command line arguments entered are correct */
    if(command_line_options(&num_threads, &img_number, argc, argv) != 0)
    {
        return -1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);

    recv_multiple_chunks_p all_png_chunks = (recv_multiple_chunks_p)malloc(sizeof(struct recv_multiple_chunks));
    all_png_chunks->png_img_chunk = (PNG_file_data_p)malloc(TOTAL_PNG_CHUNKS * sizeof(struct PNG_file_data));
    all_png_chunks->checked_png_chunk = (bool *)malloc(TOTAL_PNG_CHUNKS * sizeof(bool));
    all_png_chunks->uncompressed_data_png_images = (IDAT_uncompressed_data_p)malloc(TOTAL_PNG_CHUNKS * sizeof(struct IDAT_uncompressed_data));

    memset(all_png_chunks->png_img_chunk, 0, TOTAL_PNG_CHUNKS * sizeof(struct PNG_file_data));
    memset(all_png_chunks->uncompressed_data_png_images, 0, TOTAL_PNG_CHUNKS * sizeof(struct IDAT_uncompressed_data));
    all_png_chunks->number_of_images_received = 0;
    for(int i = 0; i < TOTAL_PNG_CHUNKS; i ++)
    {
        all_png_chunks->checked_png_chunk[i] = false;
    }

    /* figure out which image you want to receive */
    if(img_number == 1)
    {
        strcpy(all_png_chunks->url, IMG1_URL);
    }
    else if(img_number == 2)
    {
        strcpy(all_png_chunks->url, IMG2_URL);
    }
    else
    {
       strcpy(all_png_chunks->url, IMG3_URL);
    }

    pthread_t *p_tids = malloc(sizeof(pthread_t) * num_threads);

    for (int i = 0; i < num_threads; i++) {
        pthread_create(p_tids + i, NULL, send_curl, all_png_chunks);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(p_tids[i], NULL);
    }

    concatenate_png_chunks(all_png_chunks->png_img_chunk, all_png_chunks->uncompressed_data_png_images, all_png_chunks->number_of_images_received);

    /* cleaning up */
    curl_global_cleanup();

    free(p_tids);

    free(all_png_chunks->uncompressed_data_png_images);
    free(all_png_chunks->png_img_chunk);
    free(all_png_chunks->checked_png_chunk);
    free(all_png_chunks);

    return 0;
}
