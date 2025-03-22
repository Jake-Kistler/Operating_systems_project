/*************************************************
 * CS3113_Project3.cpp
 * Demonstration of dynamic memory allocation for
 * Project 3, building off your Project 2 code.
 *************************************************/

 #include <iostream>
 #include <fstream>
 #include <queue>
 #include <vector>
 #include <string>
 #include <tuple>
 #include <sstream>
 
 // -------------------------------------------------
 // MEMORY BLOCK STRUCT & FUNCTIONS
 // (From MemoryBlock.h + allocateMemoryTEST.cpp)
 // -------------------------------------------------
 struct MemoryBlock
 {
     int process_id;     // -1 if free, else the process ID occupying the block
     int start_address;  // starting memory address of this block
     int block_size;     // size in bytes
     MemoryBlock *next;  // pointer to next block in linked list
 
     MemoryBlock(int id, int start, int size)
     {
         process_id = id;
         start_address = start;
         block_size = size;
         next = nullptr;
     }
 };
 
 // Forward declarations
 void allocateMemory(MemoryBlock *&memoryHead, int process_id, int size);
 void printMemoryBlocks(MemoryBlock* memoryHead);
 void deallocateMemory(MemoryBlock *&memory_head, int process_id);
 void coalesceMemory(MemoryBlock *&memory_head);
 
 /**
  * Helper: findBlockByPID(...) 
  * Walk the linked list to find the block where process_id == pid.
  */
 MemoryBlock* findBlockByPID(MemoryBlock* head, int pid) 
 {
     MemoryBlock* current = head;
     while (current) {
         if (current->process_id == pid) {
             return current;
         }
         current = current->next;
     }
     return nullptr;
 }
 
 /**
  * allocateMemory(...) - as before:
  *   - finds first free (-1) block large enough
  *   - if found, splits block if needed, sets process_id
  *   - else prints "Process <PID> waiting...".
  */
 void allocateMemory(MemoryBlock *&memoryHead, int process_id, int size)
 {
     MemoryBlock *current = memoryHead;
 
     std::cout << "DEBUG: Trying to allocate Process " 
               << process_id << " with size " << size << "\n";
 
     while (current)
     {
         std::cout << "DEBUG: Checking block at start " 
                   << current->start_address << " with size " 
                   << current->block_size << " (Process ID: " 
                   << current->process_id << ")\n\n";
 
         // If block is free and large enough
         if (current->process_id == -1 && current->block_size >= size)
         {
             std::cout << "DEBUG: Found free block! Assigning Process " 
                       << process_id << "\n\n";
 
             int former_size = current->block_size;
             current->process_id = process_id;
             current->block_size = size;
 
             // Split leftover space if any
             if (former_size > size)
             {
                 MemoryBlock* new_block = new MemoryBlock(
                     -1,
                     current->start_address + size,
                     former_size - size
                 );
                 new_block->next = current->next;
                 current->next = new_block;
 
                 std::cout << "DEBUG: Split block. New free block at "
                           << new_block->start_address << " with size "
                           << new_block->block_size << "\n\n";
             }
 
             std::cout << "Process " << process_id
                       << " loaded into memory at address "
                       << current->start_address << " with size "
                       << size << ".\n";
             return; // Success
         }
         current = current->next;
     }
 
     // If we get here, no suitable block found
     std::cout << "Process " << process_id 
               << " waiting in NewJobQueue due to insufficient memory.\n";
 }
 
 /**
  * Print the linked list of memory blocks.
  */
 void printMemoryBlocks(MemoryBlock* memoryHead)
 {
     std::cout << "\nCurrent Memory Blocks:\n";
     MemoryBlock* current = memoryHead;
     while (current != nullptr)
     {
         std::cout << "[ "
                   << (current->process_id == -1 
                       ? "Free" 
                       : "P" + std::to_string(current->process_id))
                   << " | Start=" << current->start_address
                   << " | Size=" << current->block_size << " ] -> ";
         current = current->next;
     }
     std::cout << "NULL\n";
 }
 
 /**
  * deallocateMemory(...) - Frees the block used by a process, merges free blocks.
  */
 void deallocateMemory(MemoryBlock *&memory_head, int process_id)
 {
     MemoryBlock *current = memory_head;
 
     std::cout << "DEBUG: Trying to deallocate process " << process_id << "\n";
     while (current)
     {
         if (current->process_id == process_id)
         {
             std::cout << "DEBUG: Found process " << process_id 
                       << " at start address " << current->start_address 
                       << ", freeing memory.\n";
             current->process_id = -1;
 
             std::cout << "Process " << process_id
                       << " terminated and released memory from "
                       << current->start_address << " to "
                       << (current->start_address + current->block_size) << "\n";
 
             // Attempt to coalesce after freeing
             coalesceMemory(memory_head);
             return;
         }
         current = current->next;
     }
     // If not found
     std::cout << "WARNING: Process " << process_id 
               << " not found in memory.\n";
 }
 
 /**
  * coalesceMemory(...) - merges adjacent free blocks
  */
 void coalesceMemory(MemoryBlock *&memory_head)
 {
     MemoryBlock *current = memory_head;
     std::cout << "DEBUG: checking for memory coalescing...\n";
 
     while (current && current->next)
     {
         if (current->process_id == -1 && current->next->process_id == -1)
         {
             std::cout << "DEBUG: Merging free blocks at "
                       << current->start_address << " and "
                       << current->next->start_address << "\n";
 
             current->block_size += current->next->block_size;
             MemoryBlock* temp = current->next;
             current->next = current->next->next;
             delete temp;
 
             std::cout << "Memory coalesced at address "
                       << current->start_address << " with new size "
                       << current->block_size << "\n";
         }
         else
         {
             current = current->next;
         }
     }
 }
 
 // -------------------------------------------------
 // PROJECT-2-LIKE CPU SCHEDULING CODE
 // -------------------------------------------------
 constexpr int STATE_NEW = 1;
 constexpr int STATE_READY = 2;
 constexpr int STATE_RUNNING = 3;
 constexpr int STATE_TERMINATED = 4;
 constexpr int STATE_IOWAITING = 5;
 
 // PCB structure
 struct PCB
 {
     int process_id;
     int state;
     int program_counter;
     int instruction_base;
     int data_base;
     int memory_limit;
     int CPU_cycles_used;
     int register_value;
     int max_memory_needed;   // total mem needed
     int main_memory_base;    // base address once loaded
     std::vector<std::vector<int>> instructions; // opcode + params
 };
 
 // Some global arrays, etc.
 static const int MAX_PID = 10000;
 static int param_off_sets[MAX_PID];
 static int process_start_times[MAX_PID];
 
 int global_clock = 0;
 bool timeout_occurred = false;
 int context_switch_time, CPU_allocated;
 
 // I/O waiting queue
 std::queue<std::tuple<PCB, int, int, int>> IOWaitingQueue;
 
 std::string stateToString(int s) {
     switch(s) {
         case STATE_NEW: return "NEW";
         case STATE_READY: return "READY";
         case STATE_RUNNING: return "RUNNING";
         case STATE_TERMINATED: return "TERMINATED";
         case STATE_IOWAITING: return "IOWAITING";
     }
     return "UNKNOWN";
 }
 
 // For demonstration
 static std::vector<std::vector<int>> opcodeParamsVector = {
     {1, 2}, // compute => 2 params
     {2, 1}, // print   => 1 param
     {3, 2}, // store   => 2 params
     {4, 1}  // load    => 1 param
 };
 
 int getParamCount(int opcode)
 {
     for (auto &opinfo : opcodeParamsVector) {
         if (opinfo[0] == opcode) return opinfo[1];
     }
     return 0;
 }
 
 /**
  * executeCPU(...) - basically from Project 2
  */
 void executeCPU(int startAddress, int* mainMemory, MemoryBlock*& memoryHead)
 {
     // Reconstruct the PCB from mainMemory
     PCB process;
     process.process_id       = mainMemory[startAddress + 0];
     process.state            = mainMemory[startAddress + 1];
     process.program_counter  = mainMemory[startAddress + 2];
     process.instruction_base = mainMemory[startAddress + 3];
     process.data_base        = mainMemory[startAddress + 4];
     process.memory_limit     = mainMemory[startAddress + 5];
     process.CPU_cycles_used  = mainMemory[startAddress + 6];
     process.register_value   = mainMemory[startAddress + 7];
     process.max_memory_needed= mainMemory[startAddress + 8];
     process.main_memory_base = mainMemory[startAddress + 9];
 
     int pid = process.process_id;
     int cpu_cycles_this_run = 0;
 
     global_clock += context_switch_time;
 
     if (process.program_counter == 0) {
         process.program_counter = process.instruction_base;
         param_off_sets[pid] = 0;
         process_start_times[pid] = global_clock;
     }
 
     process.state = STATE_RUNNING;
     mainMemory[startAddress + 1] = process.state;
     mainMemory[startAddress + 2] = process.program_counter;
 
     std::cout << "Process " << pid << " has moved to Running.\n";
 
     int paramOffset = param_off_sets[pid];
 
     while (process.program_counter < process.data_base && cpu_cycles_this_run < CPU_allocated)
     {
         int opcode = mainMemory[process.program_counter];
         switch (opcode)
         {
         case 1: // compute
         {
             int iterations = mainMemory[process.data_base + paramOffset];
             int cycles = mainMemory[process.data_base + paramOffset + 1];
             std::cout << "compute\n";
 
             process.CPU_cycles_used += cycles;
             mainMemory[startAddress + 6] = process.CPU_cycles_used;
 
             cpu_cycles_this_run += cycles;
             global_clock += cycles;
             break;
         }
         case 2: // print => causes IO
         {
             int cycles = mainMemory[process.data_base + paramOffset];
             std::cout << "Process " << pid << " issued an IOInterrupt and moved to the IOWaitingQueue.\n";
 
             IOWaitingQueue.push(std::make_tuple(process, startAddress, cycles, global_clock));
 
             process.state = STATE_IOWAITING;
             mainMemory[startAddress + 1] = process.state;
             return; 
         }
         case 3: // store
         {
             int value = mainMemory[process.data_base + paramOffset];
             int address = mainMemory[process.data_base + paramOffset + 1];
 
             process.register_value = value;
             mainMemory[startAddress + 7] = process.register_value;
 
             if (address < process.memory_limit)
             {
                 mainMemory[process.main_memory_base + address] = value;
                 std::cout << "stored\n";
             }
             else
             {
                 std::cout << "store error!\n";
             }
 
             process.CPU_cycles_used++;
             mainMemory[startAddress + 6] = process.CPU_cycles_used;
 
             cpu_cycles_this_run++;
             global_clock++;
             break;
         }
         case 4: // load
         {
             int address = mainMemory[process.data_base + paramOffset];
             if (address < process.memory_limit)
             {
                 process.register_value = mainMemory[process.main_memory_base + address];
                 mainMemory[startAddress + 7] = process.register_value;
                 std::cout << "loaded\n";
             }
             else
             {
                 std::cout << "load error!\n";
             }
 
             process.CPU_cycles_used++;
             mainMemory[startAddress + 6] = process.CPU_cycles_used;
 
             cpu_cycles_this_run++;
             global_clock++;
             break;
         }
         default:
             std::cout << "Unknown opcode: " << opcode << "\n";
             break;
         }
 
         process.program_counter++;
         mainMemory[startAddress + 2] = process.program_counter;
 
         paramOffset += getParamCount(opcode);
         param_off_sets[pid] = paramOffset;
 
         // Check for time-slice
         if (cpu_cycles_this_run >= CPU_allocated && process.program_counter < process.data_base)
         {
             std::cout << "Process " << pid << " has a TimeOUT interrupt and is moved to the ReadyQueue.\n";
             process.state = STATE_READY;
             mainMemory[startAddress + 1] = process.state;
             timeout_occurred = true;
             return;
         }
     }
 
     // If we reach here, the process finished instructions
     process.program_counter = process.instruction_base - 1;
     mainMemory[startAddress + 2] = process.program_counter;
 
     process.state = STATE_TERMINATED;
     mainMemory[startAddress + 1] = process.state;
 
     int totalExecutionTime = global_clock - process_start_times[pid];
 
     // Print final stats
     std::cout << "Process ID: " << pid << "\n"
               << "State: " << stateToString(process.state) << "\n"
               << "Program Counter: " << process.program_counter << "\n"
               << "Instruction Base: " << process.instruction_base << "\n"
               << "Data Base: " << process.data_base << "\n"
               << "Memory Limit: " << process.memory_limit << "\n"
               << "CPU Cycles Used: " << process.CPU_cycles_used << "\n"
               << "Register Value: " << process.register_value << "\n"
               << "Max Memory Needed: " << process.max_memory_needed << "\n"
               << "Main Memory Base: " << process.main_memory_base << "\n"
               << "Total CPU Cycles Consumed: " << totalExecutionTime << "\n";
 
     std::cout << "Process " << pid
               << " terminated. Entered running state at: "
               << process_start_times[pid]
               << ". Terminated at: "
               << global_clock
               << ". Total Execution Time: "
               << totalExecutionTime << ".\n";
 
     // *** Deallocate memory upon termination ***
     deallocateMemory(memoryHead, pid);
     // The above line won't work as-is, because we need the actual memoryHead pointer.
     // Typically you'd do: deallocateMemory(memoryHeadGlobal, pid);
     // (So you'll want to store a global reference to memoryHead OR pass it in. 
     //  We'll fix this in the main function approach below.)
 }
 
 /**
  * checkIOWaitingQueue(...) - same logic as Project 2
  * If I/O wait is done, move process back to readyQueue.
  */
 void checkIOWaitingQueue(std::queue<int> &readyQueue, int *mainMemory)
 {
     int sizeQ = (int)IOWaitingQueue.size();
     for (int i = 0; i < sizeQ; i++)
     {
         auto frontItem = IOWaitingQueue.front();
         IOWaitingQueue.pop();
 
         PCB process       = std::get<0>(frontItem);
         int startAddress  = std::get<1>(frontItem);
         int waitTime      = std::get<2>(frontItem);
         int timeEnteredIO = std::get<3>(frontItem);
         int pid           = process.process_id;
 
         if (global_clock - timeEnteredIO >= waitTime)
         {
             int param_off_set = param_off_sets[pid];
             int cycles = mainMemory[process.data_base + param_off_set];
 
             std::cout << "print\n";
             process.CPU_cycles_used += cycles;
             mainMemory[startAddress + 6] = process.CPU_cycles_used;
 
             process.program_counter++;
             mainMemory[startAddress + 2] = process.program_counter;
 
             param_off_set += getParamCount(2); // opcode 2 => 1 param
             param_off_sets[pid] = param_off_set;
 
             process.state = STATE_READY;
             mainMemory[startAddress + 1] = process.state;
 
             std::cout << "Process " << pid 
                       << " completed I/O and is moved to the ReadyQueue.\n";
             readyQueue.push(startAddress);
         }
         else
         {
             IOWaitingQueue.push(std::make_tuple(process, startAddress, waitTime, timeEnteredIO));
         }
     }
 }
 
 /**
  * loadPCBIntoMainMemory(...) - utility to copy PCB fields + instructions
  * into the main_memory array at 'baseAddress'.
  */
 void loadPCBIntoMainMemory(const PCB& process, int* main_memory)
 {
     int base = process.main_memory_base;
 
     // Fill the "PCB region" (10 ints)
     main_memory[base + 0] = process.process_id;
     main_memory[base + 1] = process.state;
     main_memory[base + 2] = process.program_counter;
     main_memory[base + 3] = process.instruction_base;
     main_memory[base + 4] = process.data_base;
     main_memory[base + 5] = process.memory_limit;
     main_memory[base + 6] = process.CPU_cycles_used;
     main_memory[base + 7] = process.register_value;
     main_memory[base + 8] = process.max_memory_needed;
     main_memory[base + 9] = process.main_memory_base;
 
     // Now write instructions:
     int writeIndex = process.instruction_base;
     for (auto &inst : process.instructions)
     {
         main_memory[writeIndex++] = inst[0]; // opcode
     }
     for (auto &inst : process.instructions)
     {
         for (int i = 1; i < (int)inst.size(); i++)
         {
             main_memory[writeIndex++] = inst[i]; // parameters
         }
     }
 }
 
 // -------------------------------------------------
 // MAIN - Project 3 Style
 // -------------------------------------------------
 
 // We'll keep a global pointer to memoryHead so we can call
 // deallocateMemory(...) inside executeCPU(...).
 static MemoryBlock* globalMemoryHead = nullptr;
 
 /**
  * main(...)
  */
 int main(int argc, char** argv)
{
    // Read simulation parameters
    int max_memory, num_processes;
    std::cin >> max_memory >> CPU_allocated >> context_switch_time >> num_processes;

    // Set up memory management
    globalMemoryHead = new MemoryBlock(-1, 0, max_memory);

    int* main_memory = new int[max_memory];
    for (int i = 0; i < max_memory; i++)
        main_memory[i] = -1;

    for (int i = 0; i < MAX_PID; i++) {
        param_off_sets[i] = 0;
        process_start_times[i] = 0;
    }

    // Read in processes to newJobQueue
    std::queue<PCB> newJobQueue;
    for (int i = 0; i < num_processes; i++) {
        PCB process;
        std::cin >> process.process_id >> process.max_memory_needed;

        int num_instructions;
        std::cin >> num_instructions;

        process.state = STATE_NEW;
        process.memory_limit = process.max_memory_needed;
        process.program_counter = 0;
        process.CPU_cycles_used = 0;
        process.register_value = 0;

        for (int j = 0; j < num_instructions; j++) {
            int opcode;
            std::cin >> opcode;

            std::vector<int> inst;
            inst.push_back(opcode);

            int num_params = getParamCount(opcode);
            for (int k = 0; k < num_params; k++) {
                int param;
                std::cin >> param;
                inst.push_back(param);
            }

            process.instructions.push_back(inst);
        }

        newJobQueue.push(process);
    }

    std::queue<int> readyQueue;
    bool memoryChanged = true;

    // Main simulation loop
    while (!newJobQueue.empty() || !readyQueue.empty() || !IOWaitingQueue.empty())
    {
        // Try loading front job into memory only if memory has changed
        if (memoryChanged && !newJobQueue.empty())
        {
            memoryChanged = false;

            PCB& frontJob = newJobQueue.front();
            int requiredSize = frontJob.max_memory_needed + 10;

            allocateMemory(globalMemoryHead, frontJob.process_id, requiredSize);
            MemoryBlock* allocatedBlock = findBlockByPID(globalMemoryHead, frontJob.process_id);

            if (allocatedBlock && allocatedBlock->process_id == frontJob.process_id)
            {
                frontJob.main_memory_base = allocatedBlock->start_address;
                frontJob.instruction_base = allocatedBlock->start_address + 10;
                frontJob.data_base = frontJob.instruction_base + (int)frontJob.instructions.size();
                frontJob.state = STATE_READY;

                loadPCBIntoMainMemory(frontJob, main_memory);
                readyQueue.push(frontJob.main_memory_base);
                newJobQueue.pop();

                memoryChanged = true;
            }
            else
            {
                std::cout << "Insufficient memory for Process " << frontJob.process_id
                          << ". Attempting memory coalescing.\n";

                coalesceMemory(globalMemoryHead);

                allocateMemory(globalMemoryHead, frontJob.process_id, requiredSize);
                allocatedBlock = findBlockByPID(globalMemoryHead, frontJob.process_id);

                if (allocatedBlock && allocatedBlock->process_id == frontJob.process_id)
                {
                    std::cout << "Memory coalesced. Process " << frontJob.process_id << " can now be loaded.\n";

                    frontJob.main_memory_base = allocatedBlock->start_address;
                    frontJob.instruction_base = allocatedBlock->start_address + 10;
                    frontJob.data_base = frontJob.instruction_base + (int)frontJob.instructions.size();
                    frontJob.state = STATE_READY;

                    loadPCBIntoMainMemory(frontJob, main_memory);
                    readyQueue.push(frontJob.main_memory_base);
                    newJobQueue.pop();

                    memoryChanged = true;
                }
                else
                {
                    std::cout << "Process " << frontJob.process_id
                              << " waiting in NewJobQueue due to insufficient memory.\n";
                }
            }
        }

        // Run process if one is ready
        if (!readyQueue.empty())
        {
            int startAddress = readyQueue.front();
            readyQueue.pop();

            executeCPU(startAddress, main_memory, globalMemoryHead);


            if (timeout_occurred)
            {
                readyQueue.push(startAddress);
                timeout_occurred = false;
            }

            memoryChanged = true;
        }
        else
        {
            global_clock += context_switch_time;
        }

        checkIOWaitingQueue(readyQueue, main_memory);
    }

    global_clock += context_switch_time;
    std::cout << "All processes complete. Total CPU time: " << global_clock << ".\n";

    delete[] main_memory;
    return 0;
}

 