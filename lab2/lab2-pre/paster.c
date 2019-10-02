#include "cURL.h"
#include "png.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <argp.h>

#define NUM_SEGMENTS 50
#define OUTPUT_FILE "./output.png"
#define DEFAULT_URL "http://ece252-1.uwaterloo.ca:2520/image?img=1"

void cleanup(CURL* curl_handle, RECV_BUF* recv_buf );

int main( int argc, char** argv )
{

    CURLcode res;
    RECV_BUF recv_buf;
    char* url;
    simple_PNG_p pngs[NUM_SEGMENTS];
    int num_pngs_recieved = 0;

    url = malloc(strlen(DEFAULT_URL));

    /* Parse arguments */

    {
        char img;
        int n;

        char* url_pre = "http://ece252-1.uwaterloo.ca:2520/image?img=";

        for(int i = 1; i < argc; i++) {
            if (strlen(argv[i]) > 1 && i + 1 < argc && strlen(argv[i + 1]) == 1) {
                if( strcmp(argv[i], "-n") == 0 ) {
                    img = (char) argv[i + 1][0];
                    continue;
                }
            }
        }

        n = atoi(&img);

        if (n >= 1 && n <= 3) {
            strcpy(url, url_pre);
            strcat(url, &img);
        }
        else {
            strcpy(url, DEFAULT_URL);
        }

    }

    printf("URL is %s\n", url);


    /* Step 0: initialize global variables */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    for(int i = 0; i < NUM_SEGMENTS; i++){
        pngs[i] = NULL;
    }

    /*
     * Main loop for single-threaded implementation
     */

    while(num_pngs_recieved != NUM_SEGMENTS){

        /*
         * Step 1: initialize cURL
         * */

        CURL* curl_handle = curl_easy_init();
        recv_buf_init(&recv_buf, BUF_SIZE);

        if (curl_handle == NULL) {
            return -1;
        }

        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);

        /* register header call back function to process received header data */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);
        /* register write call back function to process received data */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

        /* some servers requires a user-agent field */
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        if (curl_handle == NULL) {
            fprintf(stderr, "curl_easy_init: returned NULL\n");
            return -1;
        }

        /*
         * Step 2: get image from server and check if it has been recieved
         * */

        res = curl_easy_perform(curl_handle);

        if( res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return -1;
        }

        if (pngs[recv_buf.seq] != NULL) {
            continue;
        }

        /*
         * Step 3: parse png, place inside buffer
         * */

        pngs[recv_buf.seq] = malloc(sizeof(struct simple_PNG));
        num_pngs_recieved++;

        /* Check if the png is actually a png. If not discard and continue */
        {
            U8 png_sig[PNG_SIG_SIZE];
            memcpy(png_sig, recv_buf.buf, PNG_SIG_SIZE);
            if(is_png(png_sig, PNG_SIG_SIZE) != 0) {
                cleanup(curl_handle, &recv_buf);
                continue;
            }
        }

        int parse_ret = parse_png(pngs[recv_buf.seq], recv_buf.buf);

        if( parse_ret != 0 ){
            printf("PNG Parser failed!\n");
            return -1;
        }

        /*
         * Step 4: Clean up and free variables
         * */

        cleanup(curl_handle, &recv_buf);
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
    return 0;
}

void cleanup(CURL* curl_handle, RECV_BUF* recv_buf_p ){
    curl_easy_cleanup(curl_handle);
    recv_buf_cleanup(recv_buf_p);
}
