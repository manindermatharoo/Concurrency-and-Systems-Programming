#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <argp.h>
#include <pthread.h>
#include "cURL.h"
#include "png.h"
#include "paster.h"

int main( int argc, char** argv )
{

    char* url[3];
    simple_PNG_p pngs[NUM_SEGMENTS];
    int num_pngs_recieved = 0;
    int num_threads;
    int url_index = 0;

    url[0] = malloc(strlen(DEFAULT_URL));
    url[1] = malloc(strlen(DEFAULT_URL));
    url[2] = malloc(strlen(DEFAULT_URL));

    num_threads = DEFAULT_NUM_THREADS;

    {

        int c;
        int t = 1;
        int n = 1;
        char *str = "option requires an argument";
        char* url_template = "http://ece252-%i.uwaterloo.ca:2520/image?img=%i";

        while ((c = getopt (argc, argv, "t:n:")) != -1) {
            switch (c) {

            case 't':
                t = strtoul(optarg, NULL, 10);
                if (t <= 0) {
                    fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                    return -1;
                }
                break;

            case 'n':
                n = strtoul(optarg, NULL, 10);
                if (n <= 0 || n > 3) {
                    fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
                    return -1;
                }
                break;
            }
        }

        num_threads = t;

        for(int i = 0; i < 3; i ++) {
            sprintf(url[i], url_template, i+1, n);
        }

    }

    pthread_t *p_tids = malloc(sizeof(pthread_t) * num_threads);
    thread_args in_params[num_threads];
    struct thread_ret *p_results[num_threads];

    RECV_BUF recv_bufs[num_threads];

    printf("URL is %s\n", url[0]);


    /* Step 0: initialize global variables */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    for(int i = 0; i < NUM_SEGMENTS; i++){
        pngs[i] = NULL;
    }

    while(num_pngs_recieved != NUM_SEGMENTS) {

        for (int i=0; i<num_threads; i++) {
            in_params[i].url = url[url_index];
            in_params[i].pngs = pngs;
            in_params[i].recv_buf_p = &recv_bufs[i];
            in_params[i].num_pngs = &num_pngs_recieved;
            pthread_create(p_tids + i, NULL, get_image_from_server, in_params + i); 
            url_index = (url_index + 1) % 3;
        }

        for (int i=0; i<num_threads; i++) {
            pthread_join(p_tids[i], (void **)&(p_results[i]));
        }

    }

    /*
     * Step 5: Concatenate images and write concatenated image to output file
     * */

    simple_PNG_p png_concat_p = concat_pngs(NUM_SEGMENTS, pngs);
    write_png_file(png_concat_p, OUTPUT_FILE);

    /*
     * Step 6: Clean up global variables and free up heap memory
     * */

    curl_global_cleanup();
    free(p_tids);
    return 0;
}

void cleanup (CURL* curl_handle, RECV_BUF* recv_buf_p ) {
    curl_easy_cleanup(curl_handle);
    recv_buf_cleanup(recv_buf_p);
}

void *get_image_from_server( void* arg ) {

    CURLcode res;
    struct thread_ret *p_out = malloc(sizeof(struct thread_ret));
    p_out->success = 0;

    /*
     * Step 1: initialize cURL
     * */

    thread_args *p_in = arg;

    char* url = p_in->url;
    simple_PNG_p* pngs = p_in->pngs;
    RECV_BUF* recv_buf = p_in->recv_buf_p;
    int* num_pngs_recieved = p_in->num_pngs;

    CURL* curl_handle = curl_easy_init();
    recv_buf_init(recv_buf, BUF_SIZE);

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)recv_buf);
    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)recv_buf);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    if (curl_handle != NULL) {

        /*
        * Step 2: get image from server and check if it has been recieved
        * */

        res = curl_easy_perform(curl_handle);

        if (pngs[recv_buf->seq] == NULL && res == CURLE_OK) {

            /*
            * Step 3: parse png, place inside buffer
            * */

            pngs[recv_buf->seq] = malloc(sizeof(struct simple_PNG));
            *num_pngs_recieved += 1;

            /* Check if the png is actually a png. If not discard and continue */
            {
                U8 png_sig[PNG_SIG_SIZE];
                memcpy(png_sig, recv_buf->buf, PNG_SIG_SIZE);
                if(is_png(png_sig, PNG_SIG_SIZE) == 0) {
                    parse_png(pngs[recv_buf->seq], recv_buf->buf);
                }
            }

        } else {
            p_out->success = -2;
        }

    } else {
        p_out-> success = -1;
    }


    /*
    * Step 4: Clean up and free variables
    * */

    cleanup(curl_handle, recv_buf);

    return ((void *)p_out);

}
