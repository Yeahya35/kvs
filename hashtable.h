#include "KeyValuePair.h"

//TODO fix
#define elementsType KeyValuePair

//TODO implement 

/*
    (key, value, line)
    index = hashfun(key)
    [
        .
        .
        .
        index -> (key, line) -> (key2, line2)  
        .
        .
        .s
    ]
*/
typedef struct{
    elementsType *elements;
    int capacity;
    int size;

} hash_table_t;

hash_table_t *hash_init(int capacity)
{
    hash_table_t *table = malloc(sizeof(hash_table_t));
    table->capacity = capacity;
    table->size = 0;
    table->elements = malloc(sizeof(elementsType) * capacity);
    return table;
}

void hash_insert(hash_table_t *table, long key, char* offset)
{
    int index = key % table->capacity;

    while (table->elements[index].key != 0)
    {
        index = (index + 1) % table->capacity;
    }

    table->elements[index].key = key;
    table->elements[index].value = offset; //value should be a char array
    table->size++;
}

void hash_delete(hash_table_t *table);//TODO