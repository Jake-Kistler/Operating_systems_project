// test_allocate_segments.cpp
#include <iostream>
#include <vector>
#include <string>
#define MAX_SEGMENTS 32

struct MemoryBlock
{
    int process_id;      // -1 if free
    int start_address;   // Starting address
    int size;            // Size of the block
    MemoryBlock* next;   // Next in linked list

    MemoryBlock(int id, int start, int sz)
    {
        process_id = id;
        start_address = start;
        size = sz;
        next = nullptr;
    }
};

struct PCB
{
    int process_id;
    std::string state;
    int program_counter;
    int instruction_base;
    int data_base;
    int memory_limit;
    int cpu_cycles_used;
    int register_value;
    int max_memory_needed;
    int main_memory_base;

    // Project 4 fields
    int number_of_segments;
    int segment_table_size;
    int segment_table[MAX_SEGMENTS * 2];
};

bool allocate_segments_into_pcb(
    MemoryBlock*& memory_head,
    int process_id,
    int total_memory_needed,
    PCB& pcb,
    int& segment_table_start
) {
    pcb.process_id = process_id;
    pcb.number_of_segments = 0;
    pcb.segment_table_size = 0;
    segment_table_start = -1;

    // Step 1: Coalesce adjacent free blocks
    MemoryBlock* current = memory_head;
    while (current && current->next) {
        if (current->process_id == -1 && current->next->process_id == -1 &&
            current->start_address + current->size == current->next->start_address) {
            current->size += current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }

    // Step 2: Allocate space for the segment table (13 ints)
    current = memory_head;
    MemoryBlock* prev = nullptr;

    while (current) {
        if (current->process_id == -1 && current->size >= 13) {
            segment_table_start = current->start_address;

            if (current->size == 13) {
                current->process_id = process_id;
            } else {
                MemoryBlock* newBlock = new MemoryBlock(process_id, current->start_address, 13);
                newBlock->next = current;

                if (prev) {
                    prev->next = newBlock;
                } else {
                    memory_head = newBlock;
                }

                current->start_address += 13;
                current->size -= 13;
            }
            break;
        }
        prev = current;
        current = current->next;
    }

    if (segment_table_start == -1) {
        return false;  // No space for segment table
    }

    // Step 3: Allocate non-contiguous segments
    current = memory_head;
    prev = nullptr;
    int remaining = total_memory_needed;

    while (current && remaining > 0) {
        if (current->process_id == -1) {
            int use_size = (current->size < remaining) ? current->size : remaining;

            if (pcb.number_of_segments >= MAX_SEGMENTS) {
                return false;
            }

            int idx = pcb.number_of_segments;
            pcb.segment_table[2 * idx] = current->start_address;
            pcb.segment_table[2 * idx + 1] = use_size;
            pcb.number_of_segments++;
            remaining -= use_size;

            if (use_size == current->size) {
                current->process_id = process_id;
            } else {
                MemoryBlock* newBlock = new MemoryBlock(process_id, current->start_address, use_size);
                newBlock->next = current;

                if (prev) {
                    prev->next = newBlock;
                } else {
                    memory_head = newBlock;
                }

                current->start_address += use_size;
                current->size -= use_size;
            }
        }
        prev = current;
        current = current->next;
    }

    if (remaining > 0) {
        return false;
    }

    pcb.segment_table_size = 2 * pcb.number_of_segments;
    return true;
}

void free_segments(MemoryBlock* head, int process_id)
{
    while (head)
    {
        if (head->process_id == process_id)
        {
            head->process_id = -1;
        }
        head = head->next;
    }
}

void print_memory(MemoryBlock* memory_head)
{
    std::cout << "Current Memory Layout:\n";
    while (memory_head)
    {
        std::cout << "[PID: " << memory_head->process_id
                  << ", Start: " << memory_head->start_address
                  << ", Size: " << memory_head->size << "]\n";
        memory_head = memory_head->next;
    }
    std::cout << "---------------------------------\n";
}

int main()
{
    MemoryBlock* head = new MemoryBlock(-1, 0, 10);
    head->next = new MemoryBlock(1, 10, 20);
    head->next->next = new MemoryBlock(-1, 30, 5);
    head->next->next->next = new MemoryBlock(-1, 35, 10);

    std::cout << "Before allocation:\n";
    print_memory(head);

    PCB pcb;
    int segment_table_start;

    int process_id = 2;
    int memory_needed = 12;

    bool success = allocate_segments_into_pcb(head, process_id, memory_needed, pcb, segment_table_start);

    if (success)
    {
        std::cout << "Allocation was good for process " << process_id << "\n";
        std::cout << "Segment table starts at: " << segment_table_start << "\n";
        for (int i = 0; i < pcb.number_of_segments; ++i)
        {
            std::cout << "Segment " << i << ": Start = " << pcb.segment_table[2 * i]
                      << ", Size = " << pcb.segment_table[2 * i + 1] << "\n";
        }
    }
    else
    {
        std::cout << "Allocation failed!\n";
    }

    std::cout << "\nAfter allocation:\n";
    print_memory(head);

    // Test case 2: Should fail
    std::cout << "\n---\nTrying second test case (should fail):\n";
    MemoryBlock* head2 = new MemoryBlock(-1, 0, 5);
    PCB pcb2;
    int segment_table_start2;
    bool success2 = allocate_segments_into_pcb(head2, 3, 20, pcb2, segment_table_start2);
    if (success2)
    {
        std::cout << "ERROR: Allocation should have failed but succeeded!\n";
    }
    else
    {
        std::cout << "Correctly failed to allocate memory for process 3.\n";
    }
    print_memory(head2);

    std::cout << "\nFreeing process 2...\n";
    free_segments(head, 2);
    print_memory(head);

    return 0;
}