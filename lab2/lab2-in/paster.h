#define NUM_SEGMENTS 50
#define OUTPUT_FILE "./output.png"
#define DEFAULT_URL "http://ece252-1.uwaterloo.ca:2520/image?img=1"
#define DEFAULT_NUM_THREADS 1

void cleanup(CURL* curl_handle, RECV_BUF* recv_buf_p );
void *get_image_from_server( void* arg );

typedef struct thread_args              /* thread input parameters struct */
{
    char* url;
    simple_PNG_p* pngs;
    RECV_BUF* recv_buf_p;
    int* num_pngs;

} thread_args;

struct thread_ret               /* thread return values struct   */
{
    int success;
};
