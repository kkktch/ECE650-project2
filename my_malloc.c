#include "my_malloc.h"
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <pthread.h>


static LinkList* HeadNode = NULL;
static LinkList* TailNode = NULL;
static unsigned long data_segment_size = 0;
static unsigned long data_segment_free_space_size = 0;
static size_t LLSIZE = sizeof(LinkList);
pthread_mutex_t mutex;

void eraseNode(LinkList* currNode);
void* deleteNode(LinkList* currNode);
void* divide(LinkList* currNode, size_t size);
void Traverse(LinkList* Node, int type);
void insertNode(LinkList* Node, int type);
void addNode(LinkList* Node);
void conquer(LinkList* currNode);
void conquerPrev(LinkList* currNode);
void conquerNext(LinkList* currNode);

void* ts_malloc_lock(size_t size) {
    if (size <= 0) {
        return NULL;
    }
    pthread_mutex_lock(&mutex);
    void* res = NULL;
    LinkList* currNode = HeadNode; // Start find appropriate node to allocate memory
    while (currNode != NULL) {
        if (currNode->size < size) {
            // No enough space, move to next node
            currNode = currNode->nextNode;
        }
        else {
            if (currNode->size < size + LLSIZE) {
                // Space isn't enough to divide to 2 nodes, use the whole space directly
                void* ans = deleteNode(currNode);
                res = ans;
                break;
            }
            else {
                // Space can be divided
                void* ans = divide(currNode, size);
                res = ans;
                break;
            }
        }
    }
    if (currNode == NULL) { // There is no appropriate space, use sbrk() to allocate new space
        void* tmp = sbrk(size + LLSIZE);
        LinkList* Node = tmp;
        data_segment_size += size + LLSIZE;
        eraseNode(Node);
        Node->size = size;
        Node->address = tmp + LLSIZE;
        res = Node->address;
    }
    pthread_mutex_unlock(&mutex);
    return res;
}

void ts_free_lock(void* ptr) {
    if (ptr == NULL) {
        return;
    }
    pthread_mutex_lock(&mutex);
    // Get current node
    LinkList* currNode = ptr - LLSIZE;
    currNode->isFree = 1;
    data_segment_free_space_size += currNode->size + LLSIZE;
    // Insert the free node to memory and conquer adjacent free space
    conquer(currNode);
    pthread_mutex_unlock(&mutex);
}

void* ts_malloc_nolock(size_t size) {
    // Use while loop to find the best space
    size_t minSize = __SIZE_MAX__;
    LinkList* currNode = HeadNode;
    LinkList* minNode = NULL;
    while (currNode) {
        if (currNode->size > size && currNode->size < minSize) {
            minSize = currNode->size;
            minNode = currNode;
        }
        else if (currNode->size == size){
            minNode = currNode;
            break;
        }
        currNode = currNode->nextNode;
    }
    // If there is no such space, allocate a new node
    if (minNode == NULL) {
        void* tmp = sbrk(size + LLSIZE);
        data_segment_size += size + LLSIZE;
        LinkList* Node = tmp;
        eraseNode(Node);
        Node->size = size;
        Node->address = tmp + LLSIZE;
        return Node->address;
    }
    else {
        if (minNode->size < size + LLSIZE) {
            void* ans = deleteNode(minNode);
            return ans;
        }
        else {
            void* ans = divide(minNode, size);
            return ans;
        }
    }
}

void ts_free_nolock(void* ptr) {
    // The same as ff_free()
    LinkList* currNode = ptr - LLSIZE;
    currNode->isFree = 1;
    data_segment_free_space_size += currNode->size + LLSIZE;
    conquer(currNode);
}

void eraseNode(LinkList* currNode){
    assert(currNode != NULL);
    // Erase one node from memory
    currNode->nextNode = NULL;
    currNode->prevNode = NULL;
    currNode->isFree = 0;
}

void* deleteNode(LinkList* currNode){
    if (currNode == NULL) {
        return NULL;
    }
    // Remove one node and make change to its adjacent node
    HeadNode = (currNode->prevNode == NULL) ? currNode->nextNode :
    (currNode->nextNode == NULL && currNode->prevNode == NULL) ? NULL : HeadNode;
    TailNode = (currNode->nextNode == NULL) ? currNode->prevNode :
    (currNode->nextNode == NULL && currNode->prevNode == NULL) ? NULL : TailNode;
    if (currNode->nextNode == NULL){
        if (currNode->prevNode == NULL) {
            return currNode->address;
        }
        currNode->prevNode->nextNode = NULL;
    }
    else if (currNode->prevNode == NULL){
        if (currNode->nextNode == NULL) {
            return currNode->address;
        }
        currNode->nextNode->prevNode = NULL;
    }
    else {
        currNode->prevNode->nextNode = currNode->nextNode;
        currNode->nextNode->prevNode = currNode->prevNode;
    }
    eraseNode(currNode);
    data_segment_free_space_size -= currNode->size + LLSIZE;
    return currNode->address;
    
}

void* divide(LinkList* currNode, size_t size){
    if (currNode == NULL) {
        return NULL;
    }
    // Divive a new node
    LinkList* newNode = currNode->address + size;
    data_segment_free_space_size -= size + LLSIZE;
    newNode->nextNode = currNode->nextNode;
    newNode->prevNode = currNode->prevNode;
    newNode->address = newNode + 1;
    newNode->size = currNode->size - (size + LLSIZE);
    newNode->isFree = 1;
    HeadNode = (currNode->prevNode == NULL) ? newNode : HeadNode;
    TailNode = (currNode->nextNode == NULL) ? newNode : TailNode;
    if (currNode->prevNode != NULL) {
        currNode->prevNode->nextNode = newNode;
    }
    if (currNode->nextNode != NULL) {
        currNode->nextNode->prevNode = newNode;
    }
    currNode->size = size;
    eraseNode(currNode);
    return currNode->address;
}

void Traverse(LinkList* Node, int type){
    if (Node == NULL) {
        return;
    }
    // Follow the list, find the best position to insert
    LinkList* currNode = NULL;
    if (type > 0) {
        currNode = HeadNode;
        while (currNode != TailNode && currNode->nextNode->address < Node->address) {
            currNode = currNode->nextNode;
        }
    }
    else {
        currNode = TailNode;
        while (currNode != HeadNode && currNode->prevNode->address > Node->address) {
            currNode = currNode->prevNode;
        }
    }
    LinkList* tempNext = currNode->nextNode;
    LinkList* tempPrev = currNode->prevNode;
    if (type > 0) {
        tempNext->prevNode = Node;
        currNode->nextNode = Node;
    }
    else {
        tempPrev->nextNode = Node;
        currNode->prevNode = Node;
    }
    Node->nextNode = (type > 0) ? tempNext : currNode;
    Node->prevNode = (type > 0) ? currNode : tempPrev;
}

void insertNode(LinkList* Node, int type){
    if (Node == NULL) {
        return;
    }
    // Insert the node based on the type
    if (type == 1) {
        HeadNode->prevNode = Node;
        Node->nextNode = HeadNode;
        Node->prevNode = NULL;
        HeadNode = Node;
    }
    if (type == 2) {
        TailNode->nextNode = Node;
        Node->prevNode = TailNode;
        Node->nextNode = NULL;
        TailNode = Node;
    }
    if (type == 3) {
        Traverse(Node, 1);
    }
}

void addNode(LinkList* Node) {
    if (Node == NULL) {
        return;
    }
    if (HeadNode == NULL && TailNode == NULL) {
        HeadNode = Node;
        TailNode = Node;
        return;
    }
    int type = (HeadNode->address > Node->address) ? 1 : (TailNode->address < Node->address) ? 2 : 3;
    insertNode(Node, type);
}

void conquerPrev(LinkList* currNode){
    if (currNode == NULL) {
        return;
    }
    // Conquer current node with its previous node
    currNode->prevNode->nextNode = currNode->nextNode;
    currNode->prevNode->size += currNode->size + LLSIZE;
    if (currNode->nextNode == NULL) {
        TailNode = currNode->prevNode;
    }
    else{
        currNode->nextNode->prevNode = currNode->prevNode;
        currNode->prevNode->nextNode = currNode->nextNode;
    }
}

void conquerNext(LinkList* currNode){
    if (currNode == NULL) {
        return;
    }
    // Conquer current node with its next node
    currNode->size += currNode->nextNode->size + LLSIZE;
    if (currNode->nextNode->nextNode == NULL) {
        currNode->nextNode = NULL;
        TailNode = currNode;
    }
    else {
        currNode->nextNode->prevNode = NULL;
        currNode->nextNode = currNode->nextNode->nextNode;
        currNode->nextNode->prevNode = currNode;
    }
}

void conquer(LinkList* currNode) {
    if (currNode == NULL) {
        return;
    }
    // First insert node then conquer with its adjacent node
    addNode(currNode);
    if (currNode != HeadNode && currNode->prevNode->size == (void*)currNode - currNode->prevNode->address && currNode->isFree == 1) {
        conquerPrev(currNode);
        currNode = currNode->prevNode;
    }
    if (currNode != TailNode && currNode->size == (void*)currNode->nextNode - currNode->address && currNode->isFree == 1) {
        conquerNext(currNode);
    }
}
