#include "paster.h"

#define NUM_ARGUMENTS 5
#define SEM_PROC 1
#define NUM_SEMS 2

int size_of_IDAT_formatted_data(int size)
{
    int buffer_size = (1+(4*(IMAGE_SEGMENT_WIDTH)))*IMAGE_SEGMENT_HEIGHT;
    return (buffer_size * size);
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

size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */
        fprintf(stderr, "User buffer is too small, abort...\n");
        abort();
    }
    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

/**
 * @brief calculate the actual size of RECV_BUF
 * @param size_t nbytes number of bytes that buf in RECV_BUF struct would hold
 * @return the REDV_BUF member fileds size plus the RECV_BUF buf data size
 */
int sizeof_shm_recv_buf(size_t nbytes)
{
    return (sizeof(RECV_BUF) + sizeof(char) * nbytes);
}

/**
 * @brief initialize the RECV_BUF structure.
 * @param RECV_BUF *ptr memory allocated by user to hold RECV_BUF struct
 * @param size_t nbytes the RECV_BUF buf data size in bytes
 * NOTE: caller should call sizeof_shm_recv_buf first and then allocate memory.
 *       caller is also responsible for releasing the memory.
 */

int shm_recv_buf_init(RECV_BUF *ptr, size_t nbytes)
{
    if ( ptr == NULL ) {
        return 1;
    }

    ptr->buf = (char *)malloc(BUF_SIZE);
    ptr->size = 0;
    ptr->max_size = nbytes;
    ptr->seq = -1;              /* valid seq should be non-negative */

    return 0;
}

void* send_curl(RECV_BUF* p_shm_recv_buf, int img, int img_part)
{
    /* Use current time as seed for random generator */
    srand(time(0));

    /* Initialize and setup cURL */
    CURL *curl_handle;
    CURLcode res;

    /* Randomly pick a server between 1 and 3 to fetch image */
    int upper = 3;
    int lower = 1;
    int int_num = (rand() % (upper - lower + 1)) + lower;

    /* Create the url */
    char url[256];
    sprintf(url,"%s%d%s%d%s%d","http://ece252-", int_num, ".uwaterloo.ca:2530/image?img=", img, "&part=", img_part);

    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return NULL;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)p_shm_recv_buf);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)p_shm_recv_buf);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* get it! */
    res = curl_easy_perform(curl_handle);

    if(res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    /* cleaning up */
    curl_easy_cleanup(curl_handle);

    return NULL;
}

void extract_IDAT(RECV_BUF *img, U8* IDAT_data)
{
    struct PNG_file_data* temp = malloc(sizeof(struct PNG_file_data));

    process_png_file(temp, img->buf, img->size);

    if((uncompress_IDAT_image_data(temp, IDAT_data + (img->seq * IMAGE_SIZE))) != 0)
    {
        printf("mem_inf failed \n");
    }

    free(temp);
}

void producer(sem_t* sems, pthread_mutex_t* mutex, pthread_mutex_t* mutex_stack, circular_queue *p, char *queue_buf, int img, struct int_stack *s, int buf_size)
{
    RECV_BUF* item = (RECV_BUF*)malloc(sizeof(RECV_BUF));
    int image_port = 0;
    int end_producer = 0;

    while(1)
    {
        memset(item, 0, sizeof(RECV_BUF));
        shm_recv_buf_init(item, BUF_SIZE);

        pthread_mutex_lock(mutex_stack);
        if(pop(s, &image_port) != 0)
        {
            end_producer = 1;
        }
        pthread_mutex_unlock(mutex_stack);

        if(end_producer == 1)
        {
            break;
        }

        send_curl(item, img, image_port);
        printf("Retreived image segment \n");

        if(sem_wait(&sems[0]) != 0)
        {
            perror("sem_wait on sem[0]");
            abort();
        }

        pthread_mutex_lock(mutex);
        enqueue(p, item, queue_buf);
        pthread_mutex_unlock(mutex);

        if(sem_post(&sems[1]) != 0)
        {
            perror("sem_wait on sem[0]");
            abort();
        }

        printf("Enqueued %lu bytes received in memory %p, seq=%d.\n", \
                item->size, item->buf, item->seq);

        free(item->buf);
    }

    free(item);

    return;
}

void consumer(sem_t* sems, pthread_mutex_t* mutex, circular_queue *p, char *queue_buf, int sleep_ms, U8* IDAT_data, int buf_size)
{
    RECV_BUF* ret = (RECV_BUF*)malloc(sizeof(RECV_BUF));
    int items_received = 0;

    while(items_received < 50)
    {
        memset(ret, 0, sizeof(RECV_BUF));
        shm_recv_buf_init(ret, BUF_SIZE);

        if(sem_wait(&sems[1]) != 0)
        {
            perror("sem_wait on sem[0]");
            abort();
        }

        pthread_mutex_lock(mutex);
        dequeue(p, ret, queue_buf);
        pthread_mutex_unlock(mutex);

        if(sem_post(&sems[0]) != 0)
        {
            perror("sem_wait on sem[0]");
            abort();
        }

        printf("Dequeued %lu bytes received in memory %p, seq=%d.\n", \
                ret->size, ret->buf, ret->seq);

        usleep(sleep_ms * 1000);

        extract_IDAT(ret, IDAT_data);

        printf("Completed mem_inf \n");

        free(ret->buf);
        items_received++;
    }

    free(ret);
    return;
}

int command_line_options(arguments* args, int argc, char ** argv)
{
    if(argc != (NUM_ARGUMENTS + 1))
    {
        printf("Not enough arguments provided \n");
        return -1;
    }

    for(int i = 1; i < argc; i++)
    {
        switch(i)
        {
            case 1:
                args->buf_size = atoi(argv[i]);
                if(args->buf_size <= 0)
                {
                    printf("Buffer size can't be less than 1 \n");
                    return -1;
                }
                break;
            case 2:
                args->num_producers = atoi(argv[i]);
                if(args->num_producers <= 0)
                {
                    printf("Number of producers can't be less than 1 \n");
                    return -1;
                }
                break;
            case 3:
                args->num_consumers = atoi(argv[i]);
                if(args->num_consumers != 1)
                {
                    printf("Number of consumers needs to be 1 \n");
                    return -1;
                }
                break;
            case 4:
                args->consumer_sleep_ms = atoi(argv[i]);
                if(args->consumer_sleep_ms < 0)
                {
                    printf("Sleep in microseconds must be >= 0 \n");
                    return -1;
                }
                break;
            case 5:
                args->img_number = atoi(argv[i]);
                if(args->img_number <= 0 || args->img_number > 3)
                {
                    printf("Image number must be between 1 and 3 \n");
                    return -1;
                }
                break;
            default:
                return -1;
        }

    }

    return 0;
}

int main(int argc, char** argv)
{
    double times[2];
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    arguments args;

    /* Check if command line arguments entered are correct */
    if(command_line_options(&args, argc, argv) != 0)
    {
        return -1;
    }

    int total_child_processes = args.num_producers + args.num_consumers;

    pid_t pid = 0;
    /* total number of child processes */
    pid_t child_pids[total_child_processes];

    /* Semaphores for shared memory queue, [0] - spaces, [1] - items */
    sem_t *sems;
    int shmid_sems = shmget(IPC_PRIVATE, sizeof(sem_t) * NUM_SEMS, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    if (shmid_sems == -1)
    {
        perror("shmget");
        abort();
    }

    sems = shmat(shmid_sems, NULL, 0);
    if(sems == (void *) -1)
    {
        perror("shmat");
        abort();
    }

    if(sem_init(&sems[0], SEM_PROC, args.buf_size) != 0)
    {
        perror("sem_init(sem[0])");
        abort();
    }
    if(sem_init(&sems[1], SEM_PROC, 0) != 0)
    {
        perror("sem_init(sem[1])");
        abort();
    }

    /* Create a shared memory mutex used to protect the enqueue and dequeue of queue */
    pthread_mutex_t* mutex;
    int shmid_mutex = shmget(IPC_PRIVATE, sizeof(pthread_mutex_t), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (shmid_mutex == -1 )
    {
        perror("shmget");
        abort();
    }
    mutex = shmat(shmid_mutex, NULL, 0);
    pthread_mutex_init(mutex, NULL);

    /* Create a shared memory mutex used to protect the popping of the stack */
    pthread_mutex_t* mutex_stack;
    int shmid_mutex_stack = shmget(IPC_PRIVATE, sizeof(pthread_mutex_t), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (shmid_mutex_stack == -1 )
    {
        perror("shmget");
        abort();
    }
    mutex_stack = shmat(shmid_mutex_stack, NULL, 0);
    pthread_mutex_init(mutex_stack, NULL);

    /* Create a shared memory queue */
    int shm_queue_size = sizeof_shm_queue(args.buf_size);
    int shmid_queue = shmget(IPC_PRIVATE, shm_queue_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (shmid_queue == -1 )
    {
        perror("shmget");
        abort();
    }
    circular_queue* p_queue;
    p_queue = shmat(shmid_queue, NULL, 0);
    init_shm_queue(p_queue, args.buf_size);

    /* Create a shared memory stack to let the producers know which image segment to retreive */
    int shm_stack_size = sizeof_shm_stack(TOTAL_PNG_CHUNKS);
    int shmid_stack = shmget(IPC_PRIVATE, shm_stack_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (shmid_stack == -1 ) {
        perror("shmget");
        abort();
    }
    struct int_stack *p_stack;
    p_stack = shmat(shmid_stack, NULL, 0);
    init_shm_stack(p_stack, TOTAL_PNG_CHUNKS);
    push_all(p_stack, TOTAL_PNG_CHUNKS);

    /* Create shared memory to store all the IDAT inflated buffers and lengths */
    int shm_IDAT_size = size_of_IDAT_formatted_data(TOTAL_PNG_CHUNKS);
    int shmid_IDAT = shmget(IPC_PRIVATE, shm_IDAT_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (shmid_IDAT == -1 ) {
        perror("shmget");
        abort();
    }
    U8* IDAT_data;
    IDAT_data = shmat(shmid_IDAT, NULL, 0);

    /* Create shared memory char buffer to store curl data into queue */
    int shm_RECV_buf_size = (sizeof(char) * BUF_SIZE) * (args.buf_size);
    int shmid_RECV_buf = shmget(IPC_PRIVATE, shm_RECV_buf_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    if (shmid_RECV_buf == -1 ) {
        perror("shmget");
        abort();
    }
    char* RECV_BUF_buf;
    RECV_BUF_buf = shmat(shmid_RECV_buf, NULL, 0);
    memset(RECV_BUF_buf, 0, shm_RECV_buf_size);

    init_shm_stack_RECV_BUF_buf(p_queue, RECV_BUF_buf, args.buf_size, BUF_SIZE);

    for(int i = 0; i < total_child_processes; i++)
    {
        pid = fork();

        if(pid > 0) /* parent process */
        {
            child_pids[i] = pid;
        }
        else if (pid == 0) /* child process */
        {
            if(i == total_child_processes - 1)
            {
                consumer(sems, mutex, p_queue, RECV_BUF_buf, args.consumer_sleep_ms, IDAT_data, args.buf_size);
            }
            else
            {
                producer(sems, mutex, mutex_stack, p_queue, RECV_BUF_buf, args.img_number, p_stack, args.buf_size);
            }

            if(shmdt(sems) != 0)
            {
                perror("shmdt");
                abort();
            }
            if(shmdt(p_queue) != 0)
            {
                perror("shmdt");
                abort();
            }
            if(shmdt(IDAT_data) != 0)
            {
                perror("shmdt");
                abort();
            }
            if(shmdt(mutex) != 0)
            {
                perror("shmdt");
                abort();
            }
            if(shmdt(RECV_BUF_buf) != 0)
            {
                perror("shmdt");
                abort();
            }
            break;
        }
        else
        {
            perror("fork");
            abort();
        }

    }

    if(pid > 0) /* parent process left */
    {
        for(int i = 0; i < total_child_processes; i++ )
        {
            waitpid(child_pids[i], NULL, 0);
        }

        concatenate_png_chunks(IDAT_data, TOTAL_PNG_CHUNKS);

        if(shmdt(sems) != 0)
        {
            perror("shmdt");
            abort();
        }

        if(shmctl(shmid_sems, IPC_RMID, NULL) == -1)
        {
            perror("shmctl");
            abort();
        }

        if(sem_destroy(&sems[0]) || sem_destroy(&sems[1]))
        {
            perror("sem_destroy");
            abort();
        }

        if(shmdt(p_queue) != 0)
        {
            perror("shmdt");
            abort();
        }
        if(shmdt(p_stack) != 0)
        {
            perror("shmdt");
            abort();
        }
        if(shmdt(IDAT_data) != 0)
        {
            perror("shmdt");
            abort();
        }
        if(shmdt(mutex) != 0)
        {
            perror("shmdt");
            abort();
        }
        if(shmdt(mutex_stack) != 0)
        {
            perror("shmdt");
            abort();
        }
        if(shmdt(RECV_BUF_buf) != 0)
        {
            perror("shmdt");
            abort();
        }

        if(shmctl(shmid_queue, IPC_RMID, NULL) == -1)
        {
            perror("shmctl");
            abort();
        }
        if(shmctl(shmid_stack, IPC_RMID, NULL) == -1)
        {
            perror("shmctl");
            abort();
        }
        if(shmctl(shmid_IDAT, IPC_RMID, NULL) == -1)
        {
            perror("shmctl");
            abort();
        }
        if(shmctl(shmid_mutex, IPC_RMID, NULL) == -1)
        {
            perror("shmctl");
            abort();
        }
        if(shmctl(shmid_mutex_stack, IPC_RMID, NULL) == -1)
        {
            perror("shmctl");
            abort();
        }
        if(shmctl(shmid_RECV_buf, IPC_RMID, NULL) == -1)
        {
            perror("shmctl");
            abort();
        }

        curl_global_cleanup();

        if (gettimeofday(&tv, NULL) != 0) {
            perror("gettimeofday");
            abort();
        }
        times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
        printf("paster2 execution time: %.6lf seconds\n", times[1] - times[0]);

    }

    return 0;
}
