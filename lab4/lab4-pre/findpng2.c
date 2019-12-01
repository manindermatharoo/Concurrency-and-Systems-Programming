#include "findpng2.h"

int isPNG(RECV_BUF *p_recv_buf)
{
    int file_is_png = 0;

    /* Create array that matches header of a png file */
    char png_byte_header[8];
    png_byte_header[0] = 137; //89
    png_byte_header[1] = 80;  //50
    png_byte_header[2] = 78;  //4E
    png_byte_header[3] = 71;  //47
    png_byte_header[4] = 13;  //0D
    png_byte_header[5] = 10;  //0A
    png_byte_header[6] = 26;  //1A
    png_byte_header[7] = 10;  //0A

    /* Make sure the header of the file matches a png header */
    for(int i = 0; i < 8; i++)
    {
        if(p_recv_buf->buf[i] != png_byte_header[i])
        {
            file_is_png = 1;
            break;
        }
    }

    return file_is_png;
}

htmlDocPtr mem_getdoc(char *buf, int size, const char *url)
{
    int opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | \
               HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
    htmlDocPtr doc = htmlReadMemory(buf, size, url, NULL, opts);

    if ( doc == NULL ) {
        // fprintf(stderr, "Document not parsed successfully.\n");
        return NULL;
    }
    return doc;
}

xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath)
{

    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;

    context = xmlXPathNewContext(doc);
    if (context == NULL) {
        printf("Error in xmlXPathNewContext\n");
        return NULL;
    }
    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == NULL) {
        printf("Error in xmlXPathEvalExpression\n");
        return NULL;
    }
    if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
        xmlXPathFreeObject(result);
        // printf("No result\n");
        return NULL;
    }
    return result;
}

int find_http(char *buf, int size, int follow_relative_links, const char *base_url, struct Queue* q)
{
    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar*) "//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;

    if (buf == NULL) {
        return 1;
    }

    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset (doc, xpath);
    if (result) {
        nodeset = result->nodesetval;
        for (i=0; i < nodeset->nodeNr; i++) {
            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
            if ( follow_relative_links ) {
                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *) base_url);
                xmlFree(old);
            }
            if ( href != NULL && !strncmp((const char *)href, "http", 4) ) {
                enQueue(q, (char*)href, strlen((char*)href));
                // printf("href: %s\n", href);
            }
            xmlFree(href);
        }
        xmlXPathFreeObject (result);
    }
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}
/**
 * @brief  cURL header call back function to extract image sequence number from
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

    // printf("%s", p_recv);
    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}


/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv,
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
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
    ptr->seq = -1;              /* valid seq should be positive */
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

void create_file(const char* path)
{
    FILE *fp = NULL;

    fp = fopen(path, "w");
    if (fp == NULL) {
        perror("fopen");
        return;
    }

    fclose(fp);

    return;
}

/**
 * @brief output data in memory to a file
 * @param path const char *, output file path
 * @param in  void *, input data to be written to the file
 * @param len size_t, length of the input data in bytes
 */

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "ab");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3;
    }

    if (fwrite("\n", sizeof(char), 1, fp) != 1) {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3;
    }

    return fclose(fp);
}

/**
 * @brief create a curl easy handle and set the options.
 * @param RECV_BUF *ptr points to user data needed by the curl write call back function
 * @param const char *url is the target url to fetch resoruce
 * @return a valid CURL * handle upon sucess; NULL otherwise
 * Note: the caller is responsbile for cleaning the returned curl handle
 */

CURL *easy_handle_init(RECV_BUF *ptr, const char *url)
{
    CURL *curl_handle = NULL;

    if ( ptr == NULL || url == NULL) {
        return NULL;
    }

    /* init user defined call back function buffer */
    if ( recv_buf_init(ptr, BUF_SIZE) != 0 ) {
        return NULL;
    }
    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return NULL;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)ptr);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)ptr);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ece252 lab4 crawler");

    /* follow HTTP 3XX redirects */
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    /* continue to send authentication credentials when following locations */
    curl_easy_setopt(curl_handle, CURLOPT_UNRESTRICTED_AUTH, 1L);
    /* max numbre of redirects to follow sets to 5 */
    curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5L);
    /* supports all built-in encodings */
    curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");

    /* Max time in seconds that the connection phase to the server to take */
    //curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
    /* Max time in seconds that libcurl transfer operation is allowed to take */
    //curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    /* Time out for Expect: 100-continue response in milliseconds */
    //curl_easy_setopt(curl_handle, CURLOPT_EXPECT_100_TIMEOUT_MS, 0L);

    /* Enable the cookie engine without reading any initial cookies */
    curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");
    /* allow whatever auth the proxy speaks */
    curl_easy_setopt(curl_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    /* allow whatever auth the server speaks */
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    return curl_handle;
}

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf, struct Queue* q)
{
    char fname[256];
    int follow_relative_link = 1;
    char *url = NULL;
    pid_t pid =getpid();

    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url, q);
    sprintf(fname, "./output_%d.html", pid);
    return 0;
}

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf, int* pngs_found)
{
    char *eurl = NULL;          /* effective URL */
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);
    if ( eurl != NULL) {
        if(isPNG(p_recv_buf) == 0)
        {
            (*pngs_found)++;
            char* file_name = "png_urls.txt";
            write_file(file_name, eurl, strlen(eurl));
        }
    }

    return 0;
}
/**
 * @brief process teh download data by curl
 * @param CURL *curl_handle is the curl handler
 * @param RECV_BUF p_recv_buf contains the received data.
 * @return 0 on success; non-zero otherwise
 */

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf, struct Queue* q, int* pngs_found)
{
    CURLcode res;
    char fname[256];
    pid_t pid =getpid();
    long response_code;

    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    if ( res == CURLE_OK ) {
	    // printf("Response code: %ld\n", response_code);
    }

    if ( response_code >= 400 ) {
    	// fprintf(stderr, "Error.\n");
        return 1;
    }

    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if ( res == CURLE_OK && ct != NULL ) {
    	// printf("Content-Type: %s, len=%ld\n", ct, strlen(ct));
    } else {
        // fprintf(stderr, "Failed obtain Content-Type\n");
        return 2;
    }

    if ( strstr(ct, CT_HTML) ) {
        return process_html(curl_handle, p_recv_buf, q);
    } else if ( strstr(ct, CT_PNG) ) {
        return process_png(curl_handle, p_recv_buf, pngs_found);
    } else {
        sprintf(fname, "./output_%d", pid);
    }

    return 0;
}

int command_line_options(struct thread_args* params, char *argurl, int argc, char ** argv)
{
    int option;
    char *str = "option requires an argument";
    int url_index = 1;

    int t_found = 0;
    int m_found = 0;

    if(argc < 2)
    {
        printf("Need a SEED URL. \n");
        return -1;
    }

    while ((option = getopt(argc, argv, "t:m:")) != -1)
    {
        switch (option)
        {
            case 't':
                t_found = 1;
                params->num_threads = strtoul(optarg, NULL, 10);
                if (params->num_threads <= 0)
                {
                    fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                    return -1;
                }
                break;
            case 'm':
                m_found = 1;
                params->num_images = strtoul(optarg, NULL, 10);
                if (params->num_images < 0)
                {
                    fprintf(stderr, "%s: %s > 0 -- 'n'\n", argv[0], str);
                    return -1;
                }
                break;
            default:
                return -1;
        }
    }

    if((t_found == 1) && (m_found == 1))
    {
        url_index = 5;
    }
    else if((t_found == 1) || (m_found == 1))
    {
        url_index = 3;
    }

    if(url_index >= argc) {
        fprintf(stderr, "Usage %s [-tm] SEED_URL\n", argv[0]);
        return -1;
    }
    strcpy(argurl, argv[url_index]);

    return 0;
}

void initThreadArgs(struct thread_args* params)
{
    params->num_images = 50;                                        /* number of images used */
    params->num_threads = 1;                                        /* number of threads being used; for single threaded deafult to 1 */
    params->png_urls_found = 0;                                     /* number of png urls found */
    params->q = createQueue();                                      /* create the queue */
    params->all_urls = (char**)malloc(MAX_URLS * sizeof(char*));    /* store the urls found */
    memset(params->all_urls, 0, MAX_URLS * sizeof(char*));
    params->all_urls_index = 0;                                     /* index of all urls */
}

void *retreive_urls(void *arg)
{
    struct thread_args *arguments = arg;

    CURL *curl_handle;
    RECV_BUF recv_buf;

    while((arguments->png_urls_found < arguments->num_images) && (arguments->q->front != NULL))
    {
        /* Get the most recent url */
        char* url4 = deQueue(arguments->q);

        ENTRY item;
        ENTRY *found_item;
        item.key = url4;
        item.data = 0;

        found_item = hsearch(item, FIND);

        /* Make sure the queue is not empty */
        if(url4 != NULL && found_item == NULL)
        {
            /* Update the url to be used */
            curl_handle = easy_handle_init(&recv_buf, url4);
            if ( curl_handle == NULL ) {
                fprintf(stderr, "Curl initialization failed. Exiting...\n");
                curl_global_cleanup();
                abort();
            }

            /* get it! */
            curl_easy_perform(curl_handle);

            /* process the download data */
            process_data(curl_handle, &recv_buf, arguments->q, &arguments->png_urls_found);

            char* temp_url4 = malloc(strlen(url4) * sizeof(char) + 1);
            strncpy(temp_url4, url4, strlen(url4));
            temp_url4[strlen(url4)] = '\0';

            ENTRY temp_url;
            temp_url.key = temp_url4;
            temp_url.data = 0;

            arguments->all_urls[arguments->all_urls_index++] = temp_url4;

            hsearch(temp_url, ENTER);

            free(url4);

            curl_easy_cleanup(curl_handle);
            recv_buf_cleanup(&recv_buf);
        }
        else if(url4 != NULL)
        {
            free(url4);
        }
    }

    return NULL;
}

int main( int argc, char** argv )
{
    double times[2];
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;

    char url[256];
    struct thread_args in_params;
    initThreadArgs(&in_params);

    /* Check if command line arguments entered are correct */
    if(command_line_options(&in_params, url, argc, argv) != 0)
    {
        return -1;
    }

    /* Enqueue the original url */
    enQueue(in_params.q, url, strlen(url));

    /* create an empty png_urls.txt file */
    create_file("png_urls.txt");

    /* create a hash table of 1000 possible urls */
    hcreate(MAX_URLS);

    pthread_t *p_tids = malloc(sizeof(pthread_t) * in_params.num_threads);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    for (int i = 0; i < in_params.num_threads; i++)
    {
        pthread_create(p_tids + i, NULL, retreive_urls, &in_params);
    }

    for (int i = 0; i < in_params.num_threads; i++)
    {
        pthread_join(p_tids[i], NULL);
    }

    /* cleaning up */
    curl_global_cleanup();

    free(p_tids);
    free(in_params.q);
    hdestroy();

    for(int i = 0; i < MAX_URLS; i ++)
    {
        free(in_params.all_urls[i]);
    }

    free(in_params.all_urls);

    if (gettimeofday(&tv, NULL) != 0)
    {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
    printf("findpng2 execution time: %.6lf seconds\n", times[1] - times[0]);

    return 0;
}
