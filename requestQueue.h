#include "request.h"
#include <stdbool.h>

typedef struct request_node
{
    request_t request;
    struct request_node *next;
} request_node;


// typedef struct node
// {
//     request_t request;
//     struct node *next;
// } request_node;

typedef struct request_queue 
{
    request_node *front;
    request_node *rear;
    int size;
} request_queue;


void init_queue(request_queue **queue)
{
    *queue = malloc(sizeof(request_queue));
    if (*queue == NULL)
    {
        perror("Failed to allocate memory for request queue");
        exit(EXIT_FAILURE); // Or handle the error as needed
    }
    (*queue)->front = NULL;
    (*queue)->rear = NULL;
    (*queue)->size = 0;
}



bool is_queue_empty(request_queue *queue)
{
    return (queue == NULL || queue->size == 0);
}


void remove_front_of_queue(request_queue *queue)
{
    if (!is_queue_empty(queue))
    {
        request_node *temp = queue->front;
        queue->front = queue->front->next;
        if (queue->front == NULL)
        {
            queue->rear = NULL;
        }
        free(temp);
        queue->size--;
    }
}


request_t get_front_of_queue(request_queue *queue)
{
    if (!is_queue_empty(queue))
    {
        return queue->front->request;
    }

    // Return an empty request or handle this case appropriately
    request_t empty_request;
    memset(&empty_request, 0, sizeof(request_t)); //TODO what does this do?
    return empty_request;
}


request_t dequeue(request_queue *queue)
{
    request_t request;

    if (!is_queue_empty(queue))
    {
        request = get_front_of_queue(queue);
        remove_front_of_queue(queue);
    }

    return request;

}


// do we really need this? 
request_node *create_request_node(request_t request)
{
    request_node *new_node = malloc(sizeof(request_node));

    if (new_node == NULL)
    {
        perror("Failed to allocate memory for request node");
        return NULL; // Handle memory allocation failure
    }

    new_node->request = request;
    new_node->next = NULL;
    return new_node;
}


bool enqueue(request_queue* queue, request_t request)
{
    request_node *new_node = create_request_node(request);

    if (new_node == NULL)
    {        
        printf("Failed to allocate memory for request node\n");
        return false; // Handle memory allocation failure
    }

    if (queue->rear == NULL)
    {
        // Queue is empty
        queue->front = queue->rear = new_node;
        printf("Enqueued at empty queue\n");
    } else
    {
        // Add the new node at the end of the queue and update the rear
        queue->rear->next = new_node;
        queue->rear = new_node;
        printf("Enqueued at non-empty queue\n");
    }

    queue->size++;
    return true;
}