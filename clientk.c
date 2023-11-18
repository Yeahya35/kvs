#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mqueue.h>
#include <getopt.h>

#include "request.h"

// Define constants
#define MAX_MQ_NAME_SIZE 64
#define MAX_VALUE_SIZE 1024

// Global variables
mqd_t mq_server_to_client, mq_client_to_server;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int num_client_threads;
char file_name_prefix[100];
int value_size;
char mq_name[100];
int debug_level;
long vsize;

// Request structure (similar to server)
typedef struct
{
    int type; // PUT, GET, DEL, etc.
    long key;
    char value[MAX_VALUE_SIZE]; // assuming value_size is the maximum size of value
} client_request_t;

void *fe_thread_function(void *arg)
{
    // Loop to continuously receive messages from the server
    while (1)
    {
        char response_buffer[MAX_VALUE_SIZE];
        if (mq_receive(mq_server_to_client, response_buffer, MAX_VALUE_SIZE, NULL) == -1)
        {
            perror("Error receiving response from server");
            continue; // Or break, depending on how you want to handle errors
        }

        // Process the received message and notify the appropriate client thread
        // This might involve putting the message in a shared buffer and signaling a condition variable
    }
    return NULL;
}


void *client_thread_function(void *arg)
{
    int thread_id = *(int *)arg;
    char file_name[256];
    FILE *file;

    // Construct the file name and open the file
    snprintf(file_name, sizeof(file_name), "%s%d", file_name_prefix, thread_id + 1);
    file = fopen(file_name, "r");
    if (!file)
    {
        perror("Failed to open request file");
        return NULL;
    }

    char input_line[256];
    while (fgets(input_line, sizeof(input_line), file))
    {
        request_t request;
        char request_type_str[10];

        // Parse the line to extract request details
        if (sscanf(input_line, "%s %ld %[^\n]", request_type_str, &request.key, request.value) < 2)
        {
            fprintf(stderr, "Invalid request format\n");
            continue; // Skip invalid requests
        }

        // Determine the request type
        if (strcmp(request_type_str, "PUT") == 0)
        {
            request.type = PUT;
            request.value = strdup(request.value); // Allocate and copy the value
        }
        else if (strcmp(request_type_str, "DEL") == 0)
        {
            request.type = DEL;
        }
        else if (strcmp(request_type_str, "GET") == 0)
        {
            request.type = GET;
        }
        else if (strcmp(request_type_str, "DUMP") == 0)
        {
            request.type = DUMP;
        }
        else if (strcmp(request_type_str, "QUIT") == 0)
        {
            request.type = QUIT;
        }
        else if (strcmp(request_type_str, "QUITSERVER") == 0)
        {
            request.type = QUITSERVER;
        }
        else
        {
            fprintf(stderr, "Unknown request type\n");
            continue; // Skip unknown request types
        }

        // Send request to server
        send_request_to_server(&request);

        // Here you would wait for a response from the server
        receive_response_from_server();

        // Cleanup for this request iteration
        if (request.value)
        {
            free(request.value);
        }

        if (request.type == QUIT || request.type == QUITSERVER)
        {
            break; //todo Exit the loop on QUIT or QUITSERVER
        }
    }
    fclose(file);
    return NULL;
}

void handle_interactive_mode()
{
    char input_line[256];
    client_request_t request;

    printf("Enter your requests (PUT <key> <value>, DEL <key>, GET <key>, DUMP, QUIT, or QUITSERVER):\n");

    while (fgets(input_line, sizeof(input_line), stdin))
    {
        // Reset request structure
        memset(&request, 0, sizeof(request));

        // Parse the input line to fill the request structure
        char request_type_str[10];
        if (sscanf(input_line, "%s %ld %[^\n]", request_type_str, &request.key, request.value) < 1)
        {
            fprintf(stderr, "Invalid request format\n");
            continue;
        }

        // Determine the request type
        if (strcmp(request_type_str, "PUT") == 0)
        {
            request.type = PUT;
        }
        else if (strcmp(request_type_str, "DEL") == 0)
        {
            request.type = DEL;
        }
        else if (strcmp(request_type_str, "GET") == 0)
        {
            request.type = GET;
        }
        else if (strcmp(request_type_str, "DUMP") == 0)
        {
            request.type = DUMP;
        }
        else if (strcmp(request_type_str, "QUIT") == 0)
        {
            request.type = QUIT;
        }
        else if (strcmp(request_type_str, "QUITSERVER") == 0)
        {
            request.type = QUITSERVER;
        }
        else
        {
            fprintf(stderr, "Unknown request type\n");
            continue;
        }

        // Send request to server
        send_request_to_server((request_t *)&request);

        // Wait for and process the response
        receive_response_from_server();

        // Check if the command was QUIT or QUITSERVER
        if (request.type == QUIT || request.type == QUITSERVER)
        {
            break;
        }
    }
}

void send_request_to_server(client_request_t *request)
{
    char buffer[sizeof(request->type) + sizeof(request->key) + vsize];
    memset(buffer, 0, sizeof(buffer));

    memcpy(buffer, &(request->type), sizeof(request->type));
    memcpy(buffer + sizeof(request->type), &(request->key), sizeof(request->key));
    if (request->value != NULL)
    {
        strncpy(buffer + sizeof(request->type) + sizeof(request->key), request->value, vsize - 1);
    }

    if (mq_send(mq_client_to_server, buffer, sizeof(buffer), 0) == -1)
    {
        perror("Error sending request to server");
        exit(EXIT_FAILURE);
    }
}


void receive_response_from_server()
{
    char response_buffer[MAX_VALUE_SIZE];
    memset(response_buffer, 0, sizeof(response_buffer));

    if (mq_receive(mq_server_to_client, response_buffer, sizeof(response_buffer), NULL) == -1)
    {
        perror("Error receiving response from server");
        exit(EXIT_FAILURE);
    }

    // Process the response based on your application's protocol
    printf("Response received: %s\n", response_buffer);
}

void initialize_client(int argc, char *argv[])
{
    int opt;
    num_client_threads = 0;
    strcpy(file_name_prefix, "");
    value_size = 0;
    strcpy(mq_name, "");
    debug_level = 0;

    while ((opt = getopt(argc, argv, "n:f:s:m:d:")) != -1)
    {
        switch (opt)
        {
        case 'n':
            num_client_threads = atoi(optarg);
            break;
        case 'f':
            strncpy(file_name_prefix, optarg, sizeof(file_name_prefix) - 1);
            break;
        case 's':
            value_size = atoi(optarg);
            break;
        case 'm':
            strncpy(mq_name, optarg, sizeof(mq_name) - 1);
            break;
        case 'd':
            debug_level = atoi(optarg);
            break;
        default:
            fprintf(stderr, "Usage: %s -n clicount -f fname -s vsize -m mqname -d dlevel\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // Open client-to-server message queue
    char mq_client_to_server_name[100];
    snprintf(mq_client_to_server_name, sizeof(mq_client_to_server_name), "/%s1", mq_name);
    mq_client_to_server = mq_open(mq_client_to_server_name, O_WRONLY);
    if (mq_client_to_server == (mqd_t)-1)
    {
        perror("Error opening client-to-server message queue");
        exit(EXIT_FAILURE);
    }

    // Open server-to-client message queue
    char mq_server_to_client_name[100];
    snprintf(mq_server_to_client_name, sizeof(mq_server_to_client_name), "/%s2", mq_name);
    mq_server_to_client = mq_open(mq_server_to_client_name, O_RDONLY);
    if (mq_server_to_client == (mqd_t)-1)
    {
        perror("Error opening server-to-client message queue");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{

    initialize_client(argc, argv);

    // Step 2: Create client threads
    if (num_client_threads == 0)
    {
        // Run in interactive mode
        handle_interactive_mode();
    }
    else
    {
        pthread_t threads[num_client_threads];
        int thread_args[num_client_threads];
        for (int i = 0; i < num_client_threads; i++)
        {
            thread_args[i] = i;
            if (pthread_create(&threads[i], NULL, client_thread_function, (void *)&thread_args[i]) != 0)
            {
                perror("Failed to create client thread");
                exit(EXIT_FAILURE);
            }
        }

        // Step 3: Create FE thread
        pthread_t fe_thread;
        if (pthread_create(&fe_thread, NULL, fe_thread_function, NULL) != 0)
        {
            perror("Failed to create FE thread");
            exit(EXIT_FAILURE);
        }

        // Step 4: Wait for client threads to finish
        for (int i = 0; i < num_client_threads; i++)
        {
            if (pthread_join(threads[i], NULL) != 0)
            {
                perror("Failed to join client thread");
                exit(EXIT_FAILURE);
            }
        }

        // Wait for FE thread to finish
        if (pthread_join(fe_thread, NULL) != 0)
        {
            perror("Failed to join FE thread");
            exit(EXIT_FAILURE);
        }

        // Step 5: Cleanup and exit
        mq_close(mq_client_to_server);
        mq_close(mq_server_to_client);
    }

    // Additional cleanup if necessary

    return 0;
}
