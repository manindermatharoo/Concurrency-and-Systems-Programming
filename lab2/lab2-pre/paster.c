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
                printf("Number of threads selected are %d.\n", *argt);
                break;
            case 'n':
                *argn = strtoul(optarg, NULL, 10);
                if (*argn <= 0 || *argn > 3)
                {
                    fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
                    return -1;
                }
                printf("Image chosen is %d.\n", *argn);
                break;
            default:
                return -1;
        }
    }

    return 0;
}

int main( int argc, char** argv )
{
    // Use current time as
    // seed for random generator
    srand(time(0));

    int t = 1; /* number of threads being used; for single threaded deafult to 1 */
    int n = 1; /* select the web sever; defaults to webserver 1 */
    int number_of_images_received = 0; /* total number of image chunks from the server */

    /* Check if command line arguments entered are correct */
    if(command_line_options(&t, &n, argc, argv) != 0)
    {
        return -1;
    }

    /* figure out which image you want to receive */
    char url[256];
    if(n == 1)
    {
        strcpy(url, IMG1_URL);
    }
    else if(n == 2)
    {
        strcpy(url, IMG2_URL);
    }
    else
    {
       strcpy(url, IMG3_URL);
    }

    /* Initialize and setup cURL */
    CURL *curl_handle;
    CURLcode res;
    RECV_BUF recv_buf;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return 1;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

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

    /* Create array of receive buffers for each png image chunk from the web server */
    RECV_BUF * png_images = (RECV_BUF *)malloc(TOTAL_PNG_CHUNKS * (sizeof(RECV_BUF)));
    memset(png_images, 0, TOTAL_PNG_CHUNKS * (sizeof(RECV_BUF)));
    for(int i = 0; i < TOTAL_PNG_CHUNKS; i ++)
    {
        png_images[i].seq = -1;
    }

    /* Loop until all png chunks have been stored */
    while(number_of_images_received < TOTAL_PNG_CHUNKS)
    {
        /* Randomly pick a server between 1 and 3 to fetch image */
        int upper = 3;
        int lower = 1;
        int int_num = (rand() % (upper - lower + 1)) + lower;
        char num = (char)( ((int) '0') + int_num);
        url[14] = num;

        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);

        recv_buf_init(&recv_buf, BUF_SIZE);

        /* get it! */
        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        if(png_images[recv_buf.seq].seq == -1)
        {
            png_images[recv_buf.seq].buf = recv_buf.buf;
            png_images[recv_buf.seq].size = recv_buf.size;
            png_images[recv_buf.seq].seq = recv_buf.seq;
            number_of_images_received++;

            printf("%lu bytes received in memory %p, seq=%d.\n", \
                png_images[recv_buf.seq].size, png_images[recv_buf.seq].buf, png_images[recv_buf.seq].seq);
        }
        else
        {
            recv_buf_cleanup(&recv_buf);
        }
    }

    concatenate_png_chunks(png_images, number_of_images_received);

    /* cleaning up */
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();

    for(int i = 0; i < TOTAL_PNG_CHUNKS; i ++)
    {
        free(png_images[i].buf);
    }
    free(png_images);

    return 0;
}
