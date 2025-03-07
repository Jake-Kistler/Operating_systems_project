#include "MemoryBlock.h"
#include <sstream>

// Function to allocate memory for a process
void allocateMemory(MemoryBlock *memory_head, int process_id, int size)
{
    MemoryBlock *current = memory_head;

    // need to search for a free memory block IE when process_id is -1 and see if the size of this block will work 
    while (current) // will stop when current == nullptr 
    {
        if(current->process_id == -1 && current->block_size == size)
        {
            int former_size = current->block_size; // store the orginal size of the block
            current->process_id = process_id; // assign the process ID to this block marking it for use
            current->block_size = size; // update the size of this block to the asked for size

            // we need to create a new block should there be unallocated memory
            if(former_size > size)
            {
                MemoryBlock  *new_block = new MemoryBlock(-1, current->start_address + size, 
                    former_size - size); // starts with the -1 (free) ID, it starts at the current address's start + the size, the size of this new block is the left over memory

                new_block->next = current->next; //linking it back to the linked list 
                current->next = new_block; // insert it AFTER the newly allocated block so it [allocated block] ... [new block]
            }

            std::cout << "Process " << process_id << " loaded into memory at address " << current->start_address << " with size " << size << ".\n";
            return;
        }

        current = current->next; // addvance to the next node
    } 

    std::cout << "Process " << process_id << " waiting in NewJobQueue due to insufficient memory.\n";
    
} // END allocateMemory

// Function to print the memory allocation
void printMemoryBlocks(MemoryBlock* memoryHead) 
{
    MemoryBlock* current = memoryHead;
    std::cout << "\nCurrent Memory Blocks:\n";

    while (current) 
    {
        std::ostringstream oss;

        if (current->process_id == -1) 
        {
            oss << "Free";
        } 
        else 
        {
            oss << "P" << current->process_id;
        }

        std::cout << "[ " << oss.str()
                  << " | Start=" << current->start_address
                  << " | Size=" << current->block_size << " ] -> ";

        current = current->next;
    }

    std::cout << "NULL\n";
}

int main() 
{
    // Create an initial large free memory block
    MemoryBlock* memoryHead = new MemoryBlock(-1, 0, 1000);

    // Allocate processes
    allocateMemory(memoryHead, 1, 200);
    allocateMemory(memoryHead, 2, 300);
    allocateMemory(memoryHead, 3, 250);
    allocateMemory(memoryHead, 4, 400);  // Should wait in NewJobQueue (not enough contiguous space)

    // Print the final memory state
    printMemoryBlocks(memoryHead);

    return 0;
}