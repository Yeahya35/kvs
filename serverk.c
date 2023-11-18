#define MAX_MSG_SIZE 1024
#define QUEUE_PERMISSIONS 0660
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <mqueue.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include "requestQueue.h"
#include "hashtable.h"

pthread_mutex_t lock;
pthread_cond_t cond_var;

request_queue *queue = NULL; // This will be your global request queue

hash_table_t **hash_tables;

 

// Global variables to hold command line arguments
int dcount, tcount;
long vsize;
char *fname, *mqname;
volatile int server_running = 1; // Global flag, initialized to 1

// More definitions and global variables here...
 

void initialize_data_store()
{
    // Step 1: Creating/Loading Data Files
    hash_tables = malloc(sizeof(hash_table_t *) * dcount);
    if (hash_tables == NULL)
    {
        printf("Failed to allocate memory for hash_tables\n");
        exit(EXIT_FAILURE); // or handle the error as needed
    }
    printf("Memory allocated for hash_tables\n");

    for (int i = 0; i < dcount; i++)
    {
        char file_name[256];
        sprintf(file_name, "%s%d", fname, i + 1);
        printf("Attempting to open file: %s\n", file_name);

        FILE *file = fopen(file_name, "r+b");
        if (file == NULL)
        {
            printf("File not found, creating new file: %s\n", file_name);
            file = fopen(file_name, "w+b");
        }

        if (file != NULL)
        {
            printf("File opened successfully: %s\n", file_name);
            hash_tables[i] = hash_init(1024);
            printf("hash_init called for file %s\n", file_name);
            build_index(file, i);
            fclose(file);
        }
        else
        {
            printf("Failed to create or open file: %s\n", file_name);
            // Handle error: Failed to create or open file
        }
    }
    printf("hash_init completed.\n");
}

void build_index(FILE *file, int file_number)
{
    long int key;
    char value[vsize];
    long int offset = 0; 
    while (fread(&key, sizeof(long int), 1, file) == 1)
    {
        fread(value, vsize, 1, file);
        hash_insert(hash_tables[file_number], key, offset); //long int placed in char param 
                                                            //TODO FIX 
        offset += sizeof(long int) + vsize;
    }
}

request_t get_request_from_fe()
{
    pthread_mutex_lock(&lock);
    // Assuming 'request_queue' is a shared queue where FE thread stores requests
    request_t request = dequeue(queue); 
    pthread_mutex_unlock(&lock);
    return request;
}

FILE *open_data_file(int file_index, const char *mode)
{
    char file_name[256];
    sprintf(file_name, "%s%d", fname, file_index + 1); // Construct file name

    FILE *file = fopen(file_name, mode);
    if (file == NULL)
    {
        perror("Failed to open data file");
        // Handle error appropriately
    }
    return file;
}

void put(request_t request)
{
    int file_index = request.key % dcount;          // Determine which file to use
    FILE *file = open_data_file(file_index, "r+b"); // Open the file for reading and writing

    // Find the key in the file or find an empty spot to write the new key-value pair
    // Write or update the key-value pair in the file

    fclose(file);
    // Set appropriate response message
    char response_message[MAX_MSG_SIZE];
    sprintf(response_message, "PUT successful for key %ld.", request.key);
    send_response_to_mq2(response_message);
}

void delete(request_t request)
{
    int file_index = request.key % dcount;
    FILE *file = open_data_file(file_index, "r+b");

    // Find the key in the file and mark it as deleted or remove it
    // Update the hash table/index if necessary

    fclose(file);
    // Set appropriate response message
    char response_message[MAX_MSG_SIZE];
    sprintf(response_message, "DELETE successful for key %ld.", request.key);
    send_response_to_mq2(response_message);
}

void get(request_t request)
{
    int file_index = request.key % dcount;
    FILE *file = open_data_file(file_index, "rb");

    // Find the key in the file and read its value
    // Set the value in the response message

    fclose(file);
    char response_message[MAX_MSG_SIZE];
    sprintf(response_message, "GET successful for key %ld: Value: %s", request.key, request.value);
    send_response_to_mq2(response_message);
}

void wait_on_condition_variable()
{
    pthread_mutex_lock(&lock);
    while (is_queue_empty(queue))
    {
        pthread_cond_wait(&cond_var, &lock);
    }
    pthread_mutex_unlock(&lock);
}

// Global message queue descriptor for server-to-client communication
extern mqd_t mq2;

void send_response_to_mq2(const char *response_message) {
    // Send the message to the mq2 queue
    if (mq_send(mq2, response_message, strlen(response_message) + 1, 0) == -1) {
        perror("Failed to send message to mq2");
        // Handle error appropriately
    }
}

void worker_function()
{
    while (server_running)
    {
        // Wait for work
        wait_on_condition_variable();

        // Get request from FE thread
        request_t request = get_request_from_fe();

        // Process request
        switch (request.type)
        {
        case PUT:
            put(request);
            break;
        case DEL:
            delete (request);
            break;
        case GET:
            get(request);
            break;
        default:
            // Handle unknown request type
            break;
        }

    }
}

void parse_arguments(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "d:f:t:s:m:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            dcount = atoi(optarg);
            break;
        case 'f':
            fname = optarg;
            break;
        case 't':
            tcount = atoi(optarg);
            break;
        case 's':
            vsize = atol(optarg);
            break;
        case 'm':
            mqname = optarg;
            break;
        default:
            fprintf(stderr, "Usage: %s -d dcount -f fname -t tcount -s vsize -m mqname\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

mqd_t mq1, mq2; // Message queue descriptors

void create_message_queues()
{
    struct mq_attr attr;
    char mq1_name[256], mq2_name[256];

    // Set the message queue attributes
    attr.mq_flags = 0;              // Blocking queue
    attr.mq_maxmsg = 10;            // Maximum number of messages in the queue
    attr.mq_msgsize = MAX_MSG_SIZE; // Maximum message size
    attr.mq_curmsgs = 0;            // Number of messages currently in the queue

    // Construct names for the message queues
    sprintf(mq1_name, "/%s1", mqname);
    sprintf(mq2_name, "/%s2", mqname);

    // Create or open the message queues
    mq1 = mq_open(mq1_name, O_CREAT | O_RDWR, QUEUE_PERMISSIONS, &attr);
    if (mq1 == (mqd_t)-1)
    {
        perror("mq_open mq1");
        exit(EXIT_FAILURE);
    }

    mq2 = mq_open(mq2_name, O_CREAT | O_RDWR, QUEUE_PERMISSIONS, &attr);
    if (mq2 == (mqd_t)-1)
    {
        perror("mq_open mq2");
        mq_close(mq1); // Cleanup mq1 before exiting
        exit(EXIT_FAILURE);
    }

    // Store mq1 and mq2 in global variables or pass them to relevant functions
}

void setup_synchronization_primitives()
{
    // Initialize the mutex
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        perror("pthread_mutex_init");
        exit(EXIT_FAILURE);
    }

    // Initialize the condition variable
    if (pthread_cond_init(&cond_var, NULL) != 0)
    {
        perror("pthread_cond_init");
        // Clean up the mutex before exiting
        pthread_mutex_destroy(&lock);
        exit(EXIT_FAILURE);
    }

    // Additional setup can be added here if needed
}

void *front_end_function(void *arg)
{
    // Variables to store incoming messages
    char incoming_msg[MAX_MSG_SIZE];
    request_t request;

    while (server_running)
    {
        // Read message from the mq1 (client to server message queue)
        if (mq_receive(mq1, incoming_msg, MAX_MSG_SIZE, NULL) < 0)
        {
            perror("mq_receive");
            continue;
        }

        // Parse incoming_msg to create a request object
        // For example: parse_request(incoming_msg, &request); // Implement this

        enqueue_request(request);

        // Signal a worker thread
        pthread_cond_signal(&cond_var);
    }

    return NULL;
}

void create_front_end_thread()
{
    pthread_t fe_thread;
    if (pthread_create(&fe_thread, NULL, front_end_function, NULL) != 0)
    {
        perror("Failed to create front-end thread");
        exit(EXIT_FAILURE);
    }
}

void create_worker_thread(void *(*worker_function)(void *))
{
    pthread_t thread;
    if (pthread_create(&thread, NULL, worker_function, NULL) != 0)
    {
        perror("Failed to create worker thread");
        exit(EXIT_FAILURE);
    }
    // Optionally, you can detach the thread if you don't need to join it later
    pthread_detach(thread);
}

// void enqueue_request(request_t request)
// {
//     pthread_mutex_lock(&lock);
//     printf("Enqueueing request: Type %d, Key %ld\n", request.type, request.key);
//     enqueue(queue, request);
//     pthread_mutex_unlock(&lock);
// }

void enqueue_request(request_t request) {
    pthread_mutex_lock(&lock);
    printf("Enqueueing request: Type %d, Key %ld\n", request.type, request.key);

    request_node *new_node = create_request_node(request);
    if (new_node == NULL) {
        pthread_mutex_unlock(&lock);
        printf("Failed to allocate memory for request node\n");
        return; // Handle memory allocation failure
    }

    if (queue->rear == NULL) {
        queue->front = queue->rear = new_node;
        printf("Enqueued at empty queue\n");
    } else {
        queue->rear->next = new_node;
        queue->rear = new_node;
        printf("Enqueued at non-empty queue\n");
    }
    queue->size++;
    pthread_mutex_unlock(&lock);
}

//
void cleanup_resources()
{
    // Close and unlink message queues
    mq_close(mq1);
    mq_unlink("/mq1_");
    mq_close(mq2);
    mq_unlink("/mq2");

    // Destroy mutex and condition variable
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond_var);

    // Free any other allocated resources
    // For example, free hash tables and their contents

    while (!is_queue_empty(queue))
    {
        remove_front_of_queue(queue);
    }
    free(queue);
}

void signal_handler(int sig)
{
    if (sig == SIGINT)
    {
        server_running = 0; // Assuming server_running is a global flag
    }
}

void wait_for_signal()
{
    while (server_running)
    {
        // You can use sleep to reduce CPU usage in this loop
        sleep(1);
    }
}
void setup_signal_handler()
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
}

// test function
void simulate_frontend_request()
{
    // Example requests
    request_t requests[] = {
        {PUT, 123, "Value1"},
        {PUT, 456, "Value1"},
        {PUT, 789, "Value1"}};
    int num_requests = sizeof(requests) / sizeof(request_t);

    // Simulate sending requests
    for (int i = 0; i < num_requests; i++)
    {
        enqueue_request(requests[i]);
        // Signal to worker threads that a new request has arrived
        pthread_cond_signal(&cond_var);
        // Sleep for a bit to simulate time between requests
        sleep(1);
    }
}

int main(int argc, char *argv[])
{
    // Parse arguments
    parse_arguments(argc, argv);
    printf("parsing\n");

    // Initialize data store and index
    initialize_data_store();
    printf("data-store\n");
    // Create message queues

    create_message_queues();
    printf("message\n");

    // Initialize the global request queue
    init_queue(&queue);
    printf("queue initialize\n");

    // Initialize synchronization primitives
    setup_synchronization_primitives();
    printf("synch\n");

    // Start worker threads
    for (int i = 0; i < tcount; i++)
    {
        create_worker_thread(worker_function);
    }

    // Create front-end thread
    create_front_end_thread(front_end_function);

    setup_signal_handler();

    simulate_frontend_request();

    // Server main loop
    while (server_running)
    {
        // Wait for termination signal for graceful shutdown
        // wait_for_signal();
        sleep(1);
    }

    // Cleanup before shutting down
    cleanup_resources();

    return 0;
}
