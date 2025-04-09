#include <iostream>
#include <queue>
#include <vector>
#include <fstream>
#include <string>
#include <unordered_map>
#include "MemoryBlock.h"

/*
* Project 3 asks us to change hoow jobs are loaded into memory
* Previously, we loaded directlty into the ReadyQueue if memory was aviable
* Now, we've been asked to create a NewJobQueue and when there is enough memory we will load them into the readyQueue
* In the case of not having enough memeory to use we have to options:
*   1) Wait
*   2) Coalesce memory (more on that below)
* We then contiune like normal
*
* Lets say we have 1000 memory cells
* Process 1 starts at 0 and has a size of 200 so now there are 800 free blocks to work with
* process 2 starts at 350 and has a size of 300
* process 5 starts at 750 and has a size of 250
*
* so 0-200 is used there is a gap from 200 - 350 (150 free slots)
* process 2 starts at 350 and takes 300 cells up so 350-650 is occupied now
* Then there is another free block from 650 to 750.
* we then load process 3 from 750 - 1000 and are now out of memory
*
* Say we have a new process 4 arrives and needs 180 memory cells to run,
* We don't have this space in a single cohensive block of memory and will need to
* Combine the free blocks into one unit to load process 4 and it would need to wait for memory to free up
*
*
* TO COALESCE:
* Find our unassigned blocks of memory and combine them into one unit
* so tbe block after process 1 but before process 2 is free and so is
* the block after process 2 but before process 3
*
* 150 [gap after process 1] + 100 [gap after process 2] = 250 units of free space
* If we make this a cohesvie block we can load process 4
*
* NEW STRUCTURES:
* new_job_queue<PCB> // this will store the jobs and load them into the readyQueue only when there is enough memory to do so
* Dynamic memory allocation handled / monitored by a linked list, each node has the following:
*   i) int Process_id // the id of the process -1 if free
*   ii) int start_address // where the block starts
*   iii) int block_size // size of the block
*
*/

/*
* For Project 4 we've been asked to change the memory allocation which will now allow for non-contiguous memory allocation. Meaning a process
* can be split into multiple blocks of memory.
* To make these changes we need to allow for a segment table to be created for each process and this will be stored with the PCB
* I think I can make this an unordered map and would like to do so since it will be easier to access the data (i won't have to implement the structure)
* I also won't have to bring in any new libraries to do this which is a good practice to get into.
*
*   ASSUMPTIONS:
*   1) Each process can only have at most 6 segments
*   2) The segment table + it's length must fit in a single memory hole of atleast 13 integers
*   3) If no such hole exists then the process stays in the NewJobQueue
*   4) The segment table must be contiguous in memory (no segemnt table for the segment table)
*
*   DMA CHANGES FROM PROJECT 3:
*   1) A process may be split into multiple blocks of memory
*   2) Each allocated block of memory becomes it's own segment in the segment table
*   3) If we can't find a block of memory that is large enough to load a process it stays in the NewJobQueue
*   4) Memory coalescing is now performed when searching for a free block of memory not only when a process can't be loaded
*   5) By doing this we get less internal fragmentation and more memory is grouped together when possible
*
*  NEW FUNCTIONS TO IMPLMENT:
*  1) copyProcessToMemory(int *process_logical_address, int total_logical_memory_size, int * PCB, int *main_memory) // I'll have to change this to fit my data scheme
*  2) translateLogicalAddressToPhysicalAddress(int logical_address, int *PCB)
*
* FLOW OF THE PROGRAM:
* 1) Initialize the main memory as a single large free block
* 2) For each job in the NewJobQueue:
*   a) Attempt to allocate multiple non-contiguous blocks of memory who's total size fits the process memory requirement needs
*   b) As the search is done for the block, coalesce any adjacent free blocks of memory that are found
*   c) If the memory is available:
*       i) Allocate the memory of atleast 13 integers to hold the segemnt table
*       ii) Allocate the segments of memory to the process
*       iii) Fill the segment table and complete the PCB
*       iv) Copy the segments of the PCB (segment table + metadata + instructions + data) into the allocated segemnts
*       v) Now, the PCB metadata could be stored across several blocks of memory
*       vi) Push to the ready queue
*   d) If the memory is not available:
*       i) Leave the job in the NewJobQueue
* 3) Execute jobs from the ReadyQueue following the same flow in project 2
* 4) For each instruction:
*   a) use the segment table to translate the logical address to a physical address
*   b) Validate the address
* 5) Upon job termination:
*   a) Free the memory blocks allocated to the process
*   b) update the memory linked list
* 6) After a job terminates, check the NewJobQueue for any jobs that could be loaded now
* 7) Do this until all jobs are done
*
* ** SOME THOUGHTS ON THE SEGMENT TABLE **
* The segment table from the project " Each process can have up to 6 segments, and the segment table itself, along with its length value,
* must be stored contiguously in memory(mem test shows that struct data members are stored sequentally)
*
*/


// ================================
// Function Prototypes
// ================================
struct PCB;
struct MemoryBlock;



constexpr int MAX_SEGMENTS = 6; // This magic six comes from the project 4 file

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

    // New data fields for project 4
    int number_of_segments;
    int segment_table_size; // This will be always 2 * number of segments beacuse we have the start address of the segment and the total size of the segment, this will be used later.
    int segment_table[MAX_SEGMENTS * 2]; // the array that will act as our segment table stored like [start 0, size 0, start 1, size 1...]
};

// This new struct makes the tracking for memory easier
struct segment
{
  int start_address;
  int size;
};

int global_clock = 0;
bool timeout_occurred = false;
bool memory_freed = false;
std::queue<std::tuple<PCB, int, int, int>> io_waiting_queue; // (process, start_address, param_offset, wait_time)
int context_switch_time, cpu_allocated;



std::unordered_map<int, int> opcode_params =
{
    {1, 2}, // Compute: iterations, cycles
    {2, 1}, // Print: cycles
    {3, 2}, // Store: value, address
    {4, 1}  // Load: address
};

// Key: state, Value: encoding
std::unordered_map<std::string, int> state_encoding =
{
    {"NEW", 1},
    {"READY", 2},
    {"RUNNING", 3},
    {"TERMINATED", 4},
    {"IOWAITING", 5}
};

// Key: processID, Value: instructions
std::unordered_map<int, std::vector<std::vector<int>>> process_instructions;

// Key: processID, Value: value of the global clock the first time the process entered running state
std::unordered_map<int, int> process_start_times;

// Helps keep track of each process's current parameter offset
// Key: processID, Value: param_offset
std::unordered_map<int, int> param_offsets;

// Moved so all of the args (segemnt and PCB are declared BEOFRE my prototypes)
//int allocate_memory(MemoryBlock*& memory_head, int process_id, int size);
bool allocate_segments(MemoryBlock*& memory_head,int process_id,int total_memory_needed,std::vector<segment>& out_segments,int& segment_table_start);
void free_memory(MemoryBlock*& memory_head, int* main_memory, int process_id);
void coalesce_memory(MemoryBlock*& memory_head);
void load_jobs_to_memory(std::queue<PCB>& new_job_queue,std::queue<int>& ready_queue,int* main_memory,MemoryBlock*& memory_head);
void execute_cpu(int start_address,int* main_memory,MemoryBlock*& memory_head,std::queue<PCB>& new_job_queue,std::queue<int>& ready_queue);
void check_io_waiting_queue(std::queue<int>& ready_queue, int* main_memory);


int main(int argc, char **argv)
{
    int max_memory, num_processes;
    std::queue<PCB> new_job_queue;
    std::queue<int> ready_queue;
    int* main_memory;

    std::cin >> max_memory >> cpu_allocated >> context_switch_time >> num_processes;
    main_memory = new int[max_memory];

    MemoryBlock* memory_head = new MemoryBlock(-1, 0, max_memory);

    // Initialize main_memory to -1
    for (int i = 0; i < max_memory; i++)
    {
        main_memory[i] = -1;
    }

    // Read process data
    for (int i = 0; i < num_processes; i++)
    {
        PCB process;
        int num_instructions;

        std::cin >> process.process_id
                 >> process.max_memory_needed
                 >> num_instructions;

        process.state           = "NEW";
        process.memory_limit    = process.max_memory_needed;
        process.program_counter = 0;
        process.cpu_cycles_used = 0;
        process.register_value  = 0;

        std::vector<std::vector<int>> instructions;

        for (int j = 0; j < num_instructions; j++)
        {
            std::vector<int> current_instruction;
            int opcode;
            std::cin >> opcode;
            current_instruction.push_back(opcode);

            int num_params = opcode_params[opcode];

            for (int k = 0; k < num_params; k++)
            {
                int param;
                std::cin >> param;
                current_instruction.push_back(param);
            }
            instructions.push_back(current_instruction);
        }

        // Map the instructions to process_instructions
        process_instructions[process.process_id] = instructions;
        new_job_queue.push(process);
    }

    // Load initial jobs
    load_jobs_to_memory(new_job_queue, ready_queue, main_memory, memory_head);

    // dump main_memory contents
    for (int i = 0; i < max_memory; i++)
    {
        std::cout << i << " : " << main_memory[i] << std::endl;
    }

    //  scheduling loop
    while (!ready_queue.empty() || !io_waiting_queue.empty())
    {
        if (!ready_queue.empty())
        {
            int start_address = ready_queue.front();
            ready_queue.pop();

            execute_cpu(start_address, main_memory, memory_head, new_job_queue, ready_queue);

            // If a timeout occurred, re-add the process
            if (timeout_occurred)
            {
                ready_queue.push(start_address);
                timeout_occurred = false;
            }
        }
        else
        {
            // No jobs in ready_queue, but some in io_waiting_queue
            global_clock += context_switch_time;
        }

        if (memory_freed)
        {
            load_jobs_to_memory(new_job_queue, ready_queue, main_memory, memory_head);
            memory_freed = false;
        }

        check_io_waiting_queue(ready_queue, main_memory);
    }

    global_clock += context_switch_time;
    std::cout << "Total CPU time used: " << global_clock << "." << std::endl;

    delete[] main_memory;
    return 0;
}

// ================================
// Definitions
// ================================


// This function needs to change to match what we've been asked to do in project 4. We need to combine free memory as we search
// Also, we need to store the now larger PCB. We can also track each of the free segments of memory in a strcture which will help when we have to go non contineous with our memory

//int allocate_memory(MemoryBlock*& memory_head, int process_id, int size)
//{
//    MemoryBlock* current = memory_head;
//    MemoryBlock* prev = nullptr;
//
//    while (current)
//    {
//        // Found a free block big enough
//        if (current->process_id == -1 && current->size >= size)
//        {
//            int allocated_address = current->start_address;
//            if (current->size == size)
//            {
//                current->process_id = process_id;
//            }
//            else
//            {
//                // Split
//                MemoryBlock* new_block = new MemoryBlock(process_id, current->start_address, size);
//                new_block->next = current;
//
//                if (prev)
//                {
//                    prev->next = new_block;
//                }
//                else
//                {
//                    memory_head = new_block;
//                }
//
//                current->start_address += size;
//                current->size -= size;
//                return allocated_address;
//            }
//            return allocated_address;
//        }
//        prev = current;
//        current = current->next;
//    }
//    return -1; // No block big enough
//}

void load_jobs_to_memory(std::queue<PCB>& new_job_queue,std::queue<int>& ready_queue,int* main_memory,MemoryBlock*& memory_head)
{
    int new_job_queue_size = static_cast<int>(new_job_queue.size());
    std::queue<PCB> temp_queue;

    for (int i = 0; i < new_job_queue_size; i++)
    {
        PCB process = new_job_queue.front();
        new_job_queue.pop();

        bool coalesced_for_this_process = false;
        std::vector<segment> segments;
        int segment_table_start;

        bool success = allocate_segments(memory_head, process.process_id,process.max_memory_needed,segments,segment_table_start);

        if (!success)
        {
            std::cout << "Insufficient memory for Process "
                      << process.process_id << ". Attempting memory coalescing." << std::endl;
            coalesce_memory(memory_head);

            success = allocate_segments(memory_head, process.process_id,process.max_memory_needed,segments,segment_table_start);
            coalesced_for_this_process = success;
        }

        if (!success)
        {
            std::cout << "Process " << process.process_id
                      << " waiting in NewJobQueue due to insufficient memory." << std::endl;
            temp_queue.push(process);
            continue;
        }

        // Store segment table info in PCB
        process.segment_table_size = 2 * segments.size();
        process.number_of_segments = segments.size();
        for (int j = 0; j < process.number_of_segments; ++j)
        {
            process.segment_table[2 * j] = segments[j].start_address;
            process.segment_table[2 * j + 1] = segments[j].size;
        }

        // Segment table goes in memory at segment_table_start
        process.main_memory_base = segment_table_start;

        // Determine where to write the instructions
        process.instruction_base = -1;
        for (int j = 0; j < process.number_of_segments; ++j)
        {
            int address = process.segment_table[2 * j];
            int size = process.segment_table[2 * j + 1];
            if (size >= static_cast<int>(process_instructions[process.process_id].size()))
            {
                process.instruction_base = address;
                break;
            }
        }

        // Just pick next available segment for data after instruction base
        process.data_base = -1;
        for (int j = 0; j < process.number_of_segments; ++j)
        {
            int addr = process.segment_table[2 * j];
            if (addr != process.instruction_base)
            {
                process.data_base = addr;
                break;
            }
        }

        // Flatten the PCB into memory starting at segment_table_start
        main_memory[segment_table_start + 0] = process.process_id;
        main_memory[segment_table_start + 1] = state_encoding[process.state];
        main_memory[segment_table_start + 2] = process.program_counter;
        main_memory[segment_table_start + 3] = process.instruction_base;
        main_memory[segment_table_start + 4] = process.data_base;
        main_memory[segment_table_start + 5] = process.memory_limit;
        main_memory[segment_table_start + 6] = process.cpu_cycles_used;
        main_memory[segment_table_start + 7] = process.register_value;
        main_memory[segment_table_start + 8] = process.max_memory_needed;
        main_memory[segment_table_start + 9] = process.main_memory_base;
        main_memory[segment_table_start + 10] = process.number_of_segments;
        main_memory[segment_table_start + 11] = process.segment_table_size;

        for (int k = 0; k < process.segment_table_size; ++k)
        {
            main_memory[segment_table_start + 12 + k] = process.segment_table[k];
        }

        // Store instructions
        std::vector<std::vector<int>> instrs = process_instructions[process.process_id];
        int write_index = process.instruction_base;

		for (const auto& instr : instrs)
		{
    		for (int k = 0; k < static_cast<int>(instr.size()); k++)
    		{
        		main_memory[write_index++] = instr[k]; // opcode + parameters interleaved
    		}
		}


        std::cout << "Process " << process.process_id
          << " loaded with segment table stored at physical address "
          << segment_table_start << std::endl;


        // Push to ready queue
        ready_queue.push(process.main_memory_base);
    }

    // Return failed jobs to the queue
    while (!temp_queue.empty())
    {
        new_job_queue.push(temp_queue.front());
        temp_queue.pop();
    }
}



void free_memory(MemoryBlock *&memory_head, int *main_memory, int process_id)
{
    MemoryBlock *current = memory_head;
    while (current)
    {
        if (current->process_id == process_id)
        {
            // Free the memory block
            for (int i = current->start_address; i < current->start_address + current->size; i++)
            {
                main_memory[i] = -1;
            }

            current->process_id = -1;
            return; // Done
        }
        current = current->next;
    }
}

void coalesce_memory(MemoryBlock*& head)
{
    MemoryBlock* current = head;
    while (current && current->next)
    {
        if (current->process_id == -1 && current->next->process_id == -1 &&
            current->start_address + current->size == current->next->start_address)
        {
            current->size += current->next->size;
            current->next = current->next->next;
        }
        else
        {
            current = current->next;
        }
    }
}


//void load_jobs_to_memory(std::queue<PCB>& new_job_queue,std::queue<int> &ready_queue,int *main_memory,MemoryBlock *&memory_head)
//{
//    int new_job_queue_size = static_cast<int>(new_job_queue.size());
//    std::queue<PCB> temp_queue;
//
//    for (int i = 0; i < new_job_queue_size; i++)
//    {
//        PCB process = new_job_queue.front();
//        new_job_queue.pop();
//
//        bool coalesced_for_this_process = false;
//        int total_memory_needed = process.max_memory_needed + 10;
//        int allocated_address = allocate_memory(memory_head, process.process_id, total_memory_needed);
//
//        if (allocated_address == -1)
//        {
//            std::cout << "Insufficient memory for Process "
//                      << process.process_id << ". Attempting memory coalescing." << std::endl;
//            coalesce_memory(memory_head);
//
//            allocated_address = allocate_memory(memory_head, process.process_id, total_memory_needed);
//            coalesced_for_this_process = (allocated_address != -1);
//
//            if (allocated_address == -1)
//            {
//                std::cout << "Process " << process.process_id
//                          << " waiting in NewJobQueue due to insufficient memory." << std::endl;
//                temp_queue.push(process);
//
//                for (int j = i + 1; j < new_job_queue_size; j++)
//                {
//                    temp_queue.push(new_job_queue.front());
//                    new_job_queue.pop();
//                }
//                break;
//            }
//        }
//
//        if (allocated_address != -1)
//        {
//            if (coalesced_for_this_process)
//            {
//                std::cout << "Memory coalesced. Process "
//                          << process.process_id << " can now be loaded." << std::endl;
//            }
//
//            process.main_memory_base = allocated_address;
//            process.instruction_base = allocated_address + 10;
//            process.data_base = process.instruction_base +
//                                static_cast<int>(process_instructions[process.process_id].size());
//
//            // Store PCB metadata
//            main_memory[allocated_address + 0] = process.process_id;
//            main_memory[allocated_address + 1] = state_encoding[process.state];
//            main_memory[allocated_address + 2] = process.program_counter;
//            main_memory[allocated_address + 3] = process.instruction_base;
//            main_memory[allocated_address + 4] = process.data_base;
//            main_memory[allocated_address + 5] = process.memory_limit;
//            main_memory[allocated_address + 6] = process.cpu_cycles_used;
//            main_memory[allocated_address + 7] = process.register_value;
//            main_memory[allocated_address + 8] = process.max_memory_needed;
//            main_memory[allocated_address + 9] = process.main_memory_base;
//
//            // Store instructions: [[opcode, param1, param2], ...]
//            std::vector<std::vector<int>> instrs = process_instructions[process.process_id];
//            int write_index = process.instruction_base;
//
//            // First store opcodes
//            for (const auto& instr : instrs)
//            {
//                main_memory[write_index++] = instr[0];
//            }
//
//            // Then store parameters
//            for (const auto& instr : instrs)
//            {
//                for (int k = 1; k < static_cast<int>(instr.size()); k++)
//                {
//                    main_memory[write_index++] = instr[k];
//                }
//            }
//
//            std::cout << "Process " << process.process_id
//                      << " loaded into memory at address "
//                      << allocated_address << " with size "
//                      << total_memory_needed << "." << std::endl;
//
//            // Push to ready queue
//            ready_queue.push(process.main_memory_base);
//        }
//    }
//
//    // Return failed jobs back
//    while (!temp_queue.empty())
//    {
//        new_job_queue.push(temp_queue.front());
//        temp_queue.pop();
//    }
//}

bool allocate_segments(MemoryBlock*& memory_head, int process_id, int total_memory_needed,
                       std::vector<segment>& out_segments, int& segment_table_start)
{
    out_segments.clear();
    segment_table_start = -1;

    // Step 1: Coalesce adjacent free blocks
    MemoryBlock* current = memory_head;
    while (current && current->next)
    {
        if (current->process_id == -1 && current->next->process_id == -1 &&
            current->start_address + current->size == current->next->start_address)
        {
            current->size += current->next->size;
            current->next = current->next->next;
        }
        else
        {
            current = current->next;
        }
    }

    // Step 2: Find space for the segment table (13 ints)
    current = memory_head;
    MemoryBlock* prev = nullptr;

    while (current)
    {
        if (current->process_id == -1 && current->size >= 13)
        {
            segment_table_start = current->start_address;

            if (current->size == 13)
            {
                current->process_id = process_id;
            }
            else
            {
                MemoryBlock* newBlock = new MemoryBlock(process_id, current->start_address, 13);
                newBlock->next = current;

                if (prev)
                    prev->next = newBlock;
                else
                    memory_head = newBlock;

                current->start_address += 13;
                current->size -= 13;
            }
            break;
        }
        prev = current;
        current = current->next;
    }

    if (segment_table_start == -1)
        return false;

    // Step 3: Allocate the segments
    current = memory_head;
    prev = nullptr;
    int remaining = total_memory_needed;

    while (current && remaining > 0)
    {
        if (current->process_id == -1)
        {
            int useSize = std::min(current->size, remaining);
            out_segments.push_back({current->start_address, useSize});
            remaining -= useSize;

            if (useSize == current->size)
            {
                current->process_id = process_id;
            }
            else
            {
                MemoryBlock* newBlock = new MemoryBlock(process_id, current->start_address, useSize);
                newBlock->next = current;

                if (prev)
                    prev->next = newBlock;
                else
                    memory_head = newBlock;

                current->start_address += useSize;
                current->size -= useSize;
            }
        }

        prev = current;
        current = current->next;
    }

    return (remaining == 0);
}



void execute_cpu(int start_address,int *main_memory,MemoryBlock *&memory_head,std::queue<PCB> &new_job_queue,std::queue<int> &ready_queue)
{
    PCB process;
    int cpu_cycles_this_run = 0;

    process.process_id      = main_memory[start_address + 0];
    process.state           = "READY";
    main_memory[start_address + 1] = state_encoding[process.state];
    process.program_counter = main_memory[start_address + 2];
    process.instruction_base= main_memory[start_address + 3];
    process.data_base       = main_memory[start_address + 4];
    process.memory_limit    = main_memory[start_address + 5];
    process.cpu_cycles_used = main_memory[start_address + 6];
    process.register_value  = main_memory[start_address + 7];
    process.max_memory_needed=main_memory[start_address + 8];
    process.main_memory_base= main_memory[start_address + 9];

    // Increment global clock by context switch time
    global_clock += context_switch_time;

    if (process.program_counter == 0)
    {
        process.program_counter = process.instruction_base;
        param_offsets[process.process_id] = 0;
        process_start_times[process.process_id] = global_clock;
    }

    process.state = "RUNNING";
    main_memory[start_address + 1] = state_encoding[process.state];
    main_memory[start_address + 2] = process.program_counter;
    std::cout << "Process " << process.process_id << " has moved to Running." << std::endl;

    int param_offset = param_offsets[process.process_id];

    // CPU execution loop
    while (process.program_counter < process.data_base && cpu_cycles_this_run < cpu_allocated)
    {
        int opcode = main_memory[process.program_counter];

        switch (opcode)
        {
            case 1: // Compute
            {
                int iterations = main_memory[process.data_base + param_offset];
                int cycles     = main_memory[process.data_base + param_offset + 1];
                std::cout << "compute" << std::endl;
                process.cpu_cycles_used += cycles;
                main_memory[start_address + 6] = process.cpu_cycles_used;
                cpu_cycles_this_run += cycles;
                global_clock        += cycles;
                break;
            }
            case 2: // Print
            {
                int cycles = main_memory[process.data_base + param_offset];
                std::cout << "Process " << process.process_id
                          << " issued an IOInterrupt and moved to the IOWaitingQueue." << std::endl;

                io_waiting_queue.push({process, start_address, cycles, global_clock});
                process.state = "IOWAITING";
                main_memory[start_address + 1] = state_encoding[process.state];
                return; // Let other processes run while we wait
            }
            case 3: // Store
            {
                int value   = main_memory[process.data_base + param_offset];
                int address = main_memory[process.data_base + param_offset + 1];

                process.register_value = value;
                main_memory[start_address + 7] = process.register_value;

                if (address < process.memory_limit)
                {
                    main_memory[process.main_memory_base + address] = process.register_value;
                    std::cout << "stored" << std::endl;
                }
                else
                {
                    std::cout << "store error!" << std::endl;
                }

                process.cpu_cycles_used++;
                main_memory[start_address + 6] = process.cpu_cycles_used;
                cpu_cycles_this_run++;
                global_clock++;
                break;
            }
            case 4: // Load
            {
                int address = main_memory[process.data_base + param_offset];
                if (address < process.memory_limit)
                {
                    process.register_value = main_memory[process.main_memory_base + address];
                    main_memory[start_address + 7] = process.register_value;
                    std::cout << "loaded" << std::endl;
                }
                else
                {
                    std::cout << "load error!" << std::endl;
                }

                process.cpu_cycles_used++;
                main_memory[start_address + 6] = process.cpu_cycles_used;
                cpu_cycles_this_run++;
                global_clock++;
                break;
            }
            default:
                break;
        }

        // Move to next instruction
        process.program_counter++;
        main_memory[start_address + 2] = process.program_counter;
        param_offset += opcode_params[opcode];
        param_offsets[process.process_id] = param_offset;

        // Check time-out
        if (cpu_cycles_this_run >= cpu_allocated && process.program_counter < process.data_base)
        {
            std::cout << "Process " << process.process_id
                      << " has a TimeOUT interrupt and is moved to the ReadyQueue." << std::endl;
            process.state = "READY";
            main_memory[start_address + 1] = state_encoding[process.state];
            timeout_occurred = true;
            return;
        }
    }

    // Finished instructions => set the program_counter for clarity
    process.program_counter = process.instruction_base - 1;
    process.state = "TERMINATED";
    main_memory[start_address + 2] = process.program_counter;
    main_memory[start_address + 1] = state_encoding[process.state];

    int freed_start = process.main_memory_base;
    int freed_size  = process.max_memory_needed + 10;
    free_memory(memory_head, main_memory, process.process_id);
    memory_freed = true;

    int total_execution_time = global_clock - process_start_times[process.process_id];

    // Output process info
    std::cout << "Process ID: " << process.process_id << std::endl;
    std::cout << "State: " << process.state << std::endl;
    std::cout << "Program Counter: " << process.program_counter << std::endl;
    std::cout << "Instruction Base: " << process.instruction_base << std::endl;
    std::cout << "Data Base: " << process.data_base << std::endl;
    std::cout << "Memory Limit: " << process.memory_limit << std::endl;
    std::cout << "CPU Cycles Used: " << process.cpu_cycles_used << std::endl;
    std::cout << "Register Value: " << process.register_value << std::endl;
    std::cout << "Max Memory Needed: " << process.max_memory_needed << std::endl;
    std::cout << "Main Memory Base: " << process.main_memory_base << std::endl;
    std::cout << "Total CPU Cycles Consumed: " << total_execution_time << std::endl;

    std::cout << "Process " << process.process_id
              << " terminated. Entered running state at: "
              << process_start_times[process.process_id]
              << ". Terminated at: "
              << global_clock
              << ". Total Execution Time: "
              << total_execution_time
              << "." << std::endl;

    std::cout << "Process " << process.process_id
              << " terminated and released memory from "
              << freed_start << " to "
              << (freed_start + freed_size - 1) << "." << std::endl;
}

void check_io_waiting_queue(std::queue<int>& ready_queue, int* main_memory)
{
    int queue_size = static_cast<int>(io_waiting_queue.size());
    for (int i = 0; i < queue_size; i++)
    {
        std::tuple<PCB, int, int, int> front = io_waiting_queue.front();
        PCB process = std::get<0>(front);
        int start_address = std::get<1>(front);
        int wait_time = std::get<2>(front);
        int time_entered_io = std::get<3>(front);

        io_waiting_queue.pop();

        if (global_clock - time_entered_io >= wait_time)
        {
            int param_offset = param_offsets[process.process_id];
            int cycles = main_memory[process.data_base + param_offset];

            // Execute print operation
            std::cout << "print" << std::endl;
            process.cpu_cycles_used += cycles;
            main_memory[start_address + 6] = process.cpu_cycles_used;

            // Increment program counter and paramOffset for next instruction
            process.program_counter++;
            main_memory[start_address + 2] = process.program_counter;
            param_offset += opcode_params[2];
            param_offsets[process.process_id] = param_offset;

            // Reset state to READY and context switch
            process.state = "READY";
            main_memory[start_address + 1] = state_encoding[process.state];

            std::cout << "Process " << process.process_id
                      << " completed I/O and is moved to the ReadyQueue." << std::endl;

            ready_queue.push(start_address);
        }
        else
        {
            io_waiting_queue.push(std::make_tuple(process, start_address, wait_time, time_entered_io));
        }
    }
}

int translate_logical_to_physical(int logical_address, const PCB &pcb)
{
  int remaining = logical_address;

  for(int i = 0; i < pcb.number_of_segments; i++)
    {
        int start = pcb.segment_table[2 * i]; // the start of physical address
        int size = pcb.segment_table[2 * i + 1]; // size of the segment

        if(remaining < size)
          {
              int physical_address = start + remaining;
              std::cout << "Logical address " << logical_address
                        << " translated to phsycial address " << physical_address
                        << " for process " << pcb.process_id << std::endl;
              return physical_address;
          }
        else
          {
              remaining -= size;
          }
    }

    // adress is biger than the segment size, it falls out of bounds
    std::cout << "Memory violation: address " << logical_address
              << " out of bounds for process " << pcb.process_id << std::endl;
    return -1;
}

int *build_logical_memory_array(const PCB &process, const std::vector<std::vector<int>> &instructions, int &out_bound_size)
{
  int table_size = process.segment_table_size; // this is always 2 * num_segmnets
}