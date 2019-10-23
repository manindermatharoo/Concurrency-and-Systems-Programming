#include "paster.h"

#define NUM_ARGUMENTS 5
#define SEM_PROC 1
#define NUM_SEMS 2

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

    ptr->buf = (char *)ptr + sizeof(RECV_BUF);
    ptr->size = 0;
    ptr->max_size = nbytes;
    ptr->seq = -1;              /* valid seq should be non-negative */

    return 0;
}

RECV_BUF send_curl(int img)
{
    /* Use current time as seed for random generator */
    // srand(time(0));

    /* Initialize and setup cURL */
    CURL *curl_handle;
    CURLcode res;
    RECV_BUF p_shm_recv_buf;

    shm_recv_buf_init(&p_shm_recv_buf, BUF_SIZE);

    char url[256] = "http://ece252-1.uwaterloo.ca:2530/image?img=1&part=20";

    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
    }

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&p_shm_recv_buf);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&p_shm_recv_buf);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* Randomly pick a server between 1 and 3 to fetch image */
    // int upper = 3;
    // int lower = 1;
    // int int_num = (rand() % (upper - lower + 1)) + lower;
    // int part = 2;
    // // char num = (char)( ((int) '0') + int_num);
    // char img_c = (char)( ((int) '0') + img);
    // char part_c = (char)( ((int) '0') + part);
    // // url[14] = num;
    // url[43] = img_c;
    // url[50] = part_c;

    printf("Url = %s \n", url);

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* get it! */
    res = curl_easy_perform(curl_handle);

    if(res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    /* cleaning up */
    curl_easy_cleanup(curl_handle);

    return p_shm_recv_buf;
}

void producer(sem_t* sems, circular_queue *p, int img)
{
    printf("here");
    RECV_BUF item = send_curl(img);
    printf("Here3");
    if(sem_wait(&sems[0]) != 0)
    {
        perror("sem_wait on sem[0]");
        abort();
    }
    enqueue(p, item);
    printf("Producer enqueue item \n");

    if(sem_post(&sems[1]) != 0)
    {
        perror("sem_wait on sem[0]");
        abort();
    }
    return;
}

void consumer(sem_t* sems, circular_queue *p)
{
    if(sem_wait(&sems[1]) != 0)
    {
        perror("sem_wait on sem[0]");
        abort();
    }

    RECV_BUF ret;
    dequeue(p, &ret);
    printf("Dequeued sequence = %d \n", ret.seq);

    printf("Consumer takes an item \n");

    if(sem_post(&sems[0]) != 0)
    {
        perror("sem_wait on sem[0]");
        abort();
    }
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

    /* Semaphores for shared memory queue, [0] - spaces, [1] - items, [2] - protect buffer */
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
                consumer(sems, p_queue);

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
            }
            else
            {
                printf("Child = %d \n", i);
                producer(sems, p_queue, args.img_number);

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

        if(shmctl(shmid_queue, IPC_RMID, NULL) == -1)
        {
            perror("shmctl");
            abort();
        }
    }

    curl_global_cleanup();

    return 0;
}
