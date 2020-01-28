#ifndef MY_MALLOC_H
#define MY_MALLOC_H
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h> // Library for sbrk()
typedef struct _LinkList{
    struct _LinkList* prevNode;
    struct _LinkList* nextNode;
    size_t size;
    int isFree;
    void* address;
}LinkList;

//Thread Safe malloc/free: locking version
void *ts_malloc_lock(size_t size);
void ts_free_lock(void *ptr);


//Thread Safe malloc/free: non-locking version
void *ts_malloc_nolock(size_t size);
void ts_free_nolock(void *ptr);

#endif
