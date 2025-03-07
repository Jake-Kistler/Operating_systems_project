#ifndef MEMORYBLOCK_H
#define MEMORYBLOCK_H

// this is a header for testing and will not be submitted 
#include <iostream>
struct MemoryBlock
{
    int process_id; // -1 if free otherwise holds the process ID
    int start_address; // starting memory adress
    int block_size; // size of the block
    MemoryBlock *next; // pointer to the next block in the linked list

    // constructor for this
    MemoryBlock(int id, int start, int size)
    {
        process_id = id;
        start_address = start;
        block_size = size;
        next = nullptr;
    }
};

// Function prototypes
void allocateMemory(MemoryBlock *&memoryHead, int process_id, int size);
void printMemoryBlocks(MemoryBlock* memoryHead);
void deallocateMemory(MemoryBlock *&memory_head, int process_id);

#endif // MEMORYBLOCK_H