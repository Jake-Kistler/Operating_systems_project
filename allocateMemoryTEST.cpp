#include "MemoryBlock.h"
#include <sstream>

// Function to allocate memory for a process
void allocateMemory(MemoryBlock *&memory_head, int process_id, int size)
{
    MemoryBlock *current = memory_head;

    std::cout << "DEBUG: Trying to allocate Process " << process_id << " with size " << size << "\n";


    // need to search for a free memory block IE when process_id is -1 and see if the size of this block will work 
    while (current) // will stop when current == nullptr 
    {
        std::cout << "DEBUG: Checking block at start " << current->start_address << " with size " << current->block_size << " (Process ID: " << current->process_id << ")\n\n";

        if(current->process_id == -1 && current->block_size >= size)
        {
            std::cout << "DEBUG: Found free block! Assigning Process " << process_id << "\n\n";

            int former_size = current->block_size; // store the original size of the block
            current->process_id = process_id; // assign the process ID to this block marking it for use
            current->block_size = size; // update the size of this block to the asked for size

            // we need to create a new block should there be unallocated memory
            if(former_size > size)
            {
                MemoryBlock* new_block = new MemoryBlock(-1, current->start_address + size, former_size - size);

                new_block->next = current->next; //linking it back to the linked list 

                std::cout << "DEBUG: Split block. New free block at " 
                          << new_block->start_address << " with size " << new_block->block_size << "\n\n";

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

void deallocateMemory(MemoryBlock *&memory_head, int process_id)
{
    MemoryBlock *current = memory_head;

    std::cout <<"DEBUG: Trying to deallocate process " << process_id << "\n";

    while(current)
    {
        if(current->process_id == process_id) // Found the process block number
        {
            std::cout << "DEBUG: Found process " << process_id << " at start address " << current->start_address << ", freeing memory. \n";
            current->process_id = -1; // Mark as free

            std::cout << "Process " << process_id << " terminated and released memory from " << current->start_address << " to " << (current->start_address + current->block_size) << "\n";


            //call coalesceMemory to merge adjacent free blocks
            coalesceMemory(memory_head);
            return;
            
        }

        current = current->next; // move to the next node in the list
    }

    std::cout << "WARING: process " << process_id << " not found in memory\n";
}

void coalesceMemory(MemoryBlock *&memory_head)
{
    MemoryBlock *current = memory_head;

    std::cout << "DEBUG: checking for memory coalescing...\n";

    while(current && current->next)
    {
        if(current->process_id == -1 && current->next->process_id == -1)
        {
            std::cout << "DEBUG: Merging free blocks at " << current->start_address << " and " << current->next->start_address << "\n";

            current->block_size += current->next->block_size; // expand the current block to include the next free block
            MemoryBlock *temp = current->next;
            current->next = current->next->next; // cut out the now merged block from the list
            delete temp; // free the memory

            std::cout << "Memory coalesced at address " << current->start_address << " with new size " << current->block_size << "\n";
        }
        else
        {
            current = current->next;
        }
    }
}

int main()
{
    // Create an initial large free memory block
    MemoryBlock *memoryHead = new MemoryBlock(-1, 0, 1000);

    // Allocate processes
    std::cout << "Allocation starts!\n";
    allocateMemory(memoryHead, 1, 200);
    allocateMemory(memoryHead, 2, 300);
    allocateMemory(memoryHead, 3, 250);
    allocateMemory(memoryHead, 4, 400);  // Should wait in NewJobQueue (not enough contiguous space)
    std::cout << "Allocation ends!\n";

    // Print the final memory state
    printMemoryBlocks(memoryHead);

    // Deallocate process 2 shouldn't trigger the coalescing process
    std::cout << "Deallocating process 2 starts here!\n";
    deallocateMemory(memoryHead, 2);
    printMemoryBlocks(memoryHead);
    std::cout << "We've deallocated process 2 by now!\n";

    // Deallocate process 3 which should trigger coalescing
    std::cout << "Deallocating process 3 now!\n";
    deallocateMemory(memoryHead, 3);
    printMemoryBlocks(memoryHead);
    std::cout << "And that's the show folks!\n";

    return 0;
}