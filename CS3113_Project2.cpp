/*************************************************
 * Merged Code: CS3113_Project2 + MemoryBlock.h + allocateMemoryTEST.cpp
 *************************************************/

 #include <iostream>
 #include <queue>
 #include <vector>
 #include <string>
 #include <tuple>
 #include <sstream>
 #include <fstream>
 
 /*************************************************
  * MemoryBlock + Memory Management Functions
  * (from MemoryBlock.h and allocateMemoryTEST.cpp)
  *************************************************/
 
 // This struct tracks a block of memory:
 //  - process_id: which process occupies this block (-1 if free)
 //  - start_address: where this block begins in main memory
 //  - block_size: how large (in bytes) this block is
 //  - next: pointer to the next block in the linked list
 struct MemoryBlock
 {
     int process_id;     
     int start_address;  
     int block_size;     
     MemoryBlock *next;  
 
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
  * Allocate a memory block for a given process.
  * Looks for the first free block (-1) that has at least `size` capacity.
  * If found, occupies that block and splits off any remaining free space.
  * If not found, prints that the process is waiting.
  */
 void allocateMemory(MemoryBlock *&memoryHead, int process_id, int size)
 {
     MemoryBlock *current = memoryHead;
 
     std::cout << "DEBUG: Trying to allocate Process " << process_id 
               << " with size " << size << "\n";
 
     while (current)
     {
         std::cout << "DEBUG: Checking block at start " << current->start_address 
                   << " with size " << current->block_size 
                   << " (Process ID: " << current->process_id << ")\n\n";
 
         // If free and large enough
         if (current->process_id == -1 && current->block_size >= size)
         {
             std::cout << "DEBUG: Found free block! Assigning Process " 
                       << process_id << "\n\n";
 
             int former_size = current->block_size;
             current->process_id = process_id; 
             current->block_size = size;
 
             // Split off leftover space as a new free block
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
                           << new_block->start_address 
                           << " with size " << new_block->block_size << "\n\n";
             }
 
             std::cout << "Process " << process_id 
                       << " loaded into memory at address " 
                       << current->start_address 
                       << " with size " << size << ".\n";
             return;
         }
 
         current = current->next;
     }
 
     // If we reach here, no suitable free block was found
     std::cout << "Process " << process_id 
               << " waiting in NewJobQueue due to insufficient memory.\n";
 }
 
 /**
  * Print the linked list of memory blocks for debugging or status display.
  */
 void printMemoryBlocks(MemoryBlock* memoryHead)
 {
     MemoryBlock* current = memoryHead;
     std::cout << "\nCurrent Memory Blocks:\n";
 
     while (current != nullptr)
     {
         std::cout << "[ " 
                   << (current->process_id == -1 
                       ? "Free" 
                       : "P" + std::to_string(current->process_id))
                   << " | Start=" << current->start_address
                   << " | Size=" << current->block_size 
                   << " ] -> ";
         current = current->next;
     }
     std::cout << "NULL\n";
 }
 
 /**
  * Deallocate a block used by a process. Marks it free (process_id = -1),
  * prints a termination message, and coalesces memory blocks if possible.
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
 
     std::cout << "WARNING: Process " << process_id << " not found in memory.\n";
 }
 
 /**
  * Merge adjacent free blocks into one larger block if they are consecutive.
  */
 void coalesceMemory(MemoryBlock *&memory_head)
 {
     MemoryBlock *current = memory_head;
     std::cout << "DEBUG: checking for memory coalescing...\n";
 
     while (current && current->next)
     {
         // If current and next are free, merge them
         if (current->process_id == -1 && current->next->process_id == -1)
         {
             std::cout << "DEBUG: Merging free blocks at " 
                       << current->start_address << " and " 
                       << current->next->start_address << "\n";
 
             current->block_size += current->next->block_size;
             MemoryBlock *temp = current->next;
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
 
 /*************************************************
  * Existing Code from CS3113_Project2.cpp
  *************************************************/
 
 // State codes
 constexpr int STATE_NEW = 1;
 constexpr int STATE_READY = 2;
 constexpr int STATE_RUNNING = 3;
 constexpr int STATE_TERMINATED = 4;
 constexpr int STATE_IOWAITING = 5;
 
 // Basic PCB structure
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
     int max_memory_needed;
     int main_memory_base;
     std::vector<std::vector<int>> instructions;
 };
 
 // Global arrays for process offsets
 static const int MAX_PID = 10000;
 static int param_off_sets[MAX_PID];
 static int process_start_times[MAX_PID];
 
 // Some global variables for demonstration
 int global_clock = 0;
 bool timeout_occurred = false;
 int context_switch_time, CPU_allocated;
 
 /*
  * This queue (IOWaitingQueue) is a queue of tuples: (PCB, startAddress, waitTime, timeEnteredIO).
  * We'll keep track of processes waiting for IO here.
  */
 std::queue<std::tuple<PCB, int, int, int>> IOWaitingQueue;
 
 std::string stateToString(int state_code)
 {
     switch (state_code)
     {
     case STATE_NEW:       return "NEW";
     case STATE_READY:     return "READY";
     case STATE_RUNNING:   return "RUNNING";
     case STATE_TERMINATED:return "TERMINATED";
     case STATE_IOWAITING: return "IOWAITING";
     }
     return "UNKNOWN";
 }
 
 /**
  * Return how many parameters each opcode expects.
  */
 static std::vector<std::vector<int>> opcodeParamsVector = {
     {1, 2}, // compute => 2 params
     {2, 1}, // print   => 1 param
     {3, 2}, // store   => 2 params
     {4, 1}, // load    => 1 param
 };
 
 int getParamCount(int opcode)
 {
     for (auto &opinfo : opcodeParamsVector)
     {
         if (opinfo[0] == opcode)
             return opinfo[1];
     }
     return 0;
 }
 
 /**
  * Execution function that simulates a CPU running instructions in mainMemory.
  * For demonstration, it includes a basic time-slice approach and I/O blocking.
  */
 void executeCPU(int startAddress, int *mainMemory)
 {
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
 
     if (process.program_counter == 0)
     {
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
                 int cycles     = mainMemory[process.data_base + paramOffset + 1];
 
                 std::cout << "compute\n";
 
                 process.CPU_cycles_used += cycles;
                 mainMemory[startAddress + 6] = process.CPU_cycles_used;
 
                 cpu_cycles_this_run += cycles;
                 global_clock        += cycles;
 
                 break;
             }
             case 2: // print => IO
             {
                 int cycles = mainMemory[process.data_base + paramOffset];
                 std::cout << "Process " << pid 
                           << " issued an IOInterrupt and moved to the IOWaitingQueue.\n";
 
                 IOWaitingQueue.push(std::make_tuple(process, startAddress, cycles, global_clock));
 
                 process.state = STATE_IOWAITING;
                 mainMemory[startAddress + 1] = process.state;
                 return;
             }
             case 3: // store
             {
                 int value   = mainMemory[process.data_base + paramOffset];
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
                 break;
         }
 
         process.program_counter++;
         mainMemory[startAddress + 2] = process.program_counter;
 
         paramOffset += getParamCount(opcode);
         param_off_sets[pid] = paramOffset;
 
         // Check for time-slice expiration
         if (cpu_cycles_this_run >= CPU_allocated && process.program_counter < process.data_base)
         {
             std::cout << "Process " << pid 
                       << " has a TimeOUT interrupt and is moved to the ReadyQueue.\n";
             process.state = STATE_READY;
 
             mainMemory[startAddress + 1] = process.state;
             timeout_occurred = true;
             return;
         }
     }
 
     // If we get here, the process has finished instructions
     process.program_counter = process.instruction_base - 1;
     mainMemory[startAddress + 2] = process.program_counter;
 
     process.state = STATE_TERMINATED;
     mainMemory[startAddress + 1] = process.state;
 
     int totalExecutionTime = global_clock - process_start_times[pid];
 
     // Print final process stats
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
 }
 
 /**
  * Check the I/O waiting queue and see if any processes have finished waiting.
  * If so, move them back to the ReadyQueue.
  */
 void checkIOWaitingQueue(std::queue<int> &readyQueue, int *mainMemory)
 {
     int size = (int)IOWaitingQueue.size();
     for (int i = 0; i < size; i++)
     {
         auto frontItem = IOWaitingQueue.front();
         IOWaitingQueue.pop();
 
         PCB process         = std::get<0>(frontItem);
         int startAddress    = std::get<1>(frontItem);
         int waitTime        = std::get<2>(frontItem);
         int timeEnteredIO   = std::get<3>(frontItem);
         int pid             = process.process_id;
 
         // If the I/O wait has elapsed, finalize the "print" or I/O
         if (global_clock - timeEnteredIO >= waitTime)
         {
             int param_off_set = param_off_sets[pid];
             int cycles        = mainMemory[process.data_base + param_off_set];
 
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
  * Basic function to load all jobs into memory at once,
  * ignoring dynamic memory. (Project 3 would replace this
  * with calls to `allocateMemory(...)`, etc.)
  * 
  * This version is largely from Project2 but you can adapt it
  * to use your new dynamic memory allocation for Project3.
  */
 void loadJobsToMemory(std::queue<PCB> &newJobQueue, std::queue<int> &readyQueue, int *mainMemory, int maxMemory)
 {
     int memoryIndex = 0;
     while (!newJobQueue.empty())
     {
         PCB process = newJobQueue.front();
         newJobQueue.pop();
 
         // For demonstration, just do a naive "if it fits" approach
         if (memoryIndex + process.max_memory_needed > maxMemory)
         {
             std::cout << "Not enough memory to load Process " << process.process_id << "\n";
             continue;
         }
 
         // "Load" the PCB into mainMemory
         process.main_memory_base = memoryIndex;
         process.instruction_base = memoryIndex + 10;
         process.data_base        = process.instruction_base + (int)process.instructions.size();
 
         mainMemory[memoryIndex + 0] = process.process_id;
         mainMemory[memoryIndex + 1] = process.state;
         mainMemory[memoryIndex + 2] = process.program_counter;
         mainMemory[memoryIndex + 3] = process.instruction_base;
         mainMemory[memoryIndex + 4] = process.data_base;
         mainMemory[memoryIndex + 5] = process.memory_limit;
         mainMemory[memoryIndex + 6] = process.CPU_cycles_used;
         mainMemory[memoryIndex + 7] = process.register_value;
         mainMemory[memoryIndex + 8] = process.max_memory_needed;
         mainMemory[memoryIndex + 9] = process.main_memory_base;
 
         int writeIndex = process.instruction_base;
         for (auto &instr : process.instructions)
         {
             mainMemory[writeIndex++] = instr[0];
         }
         for (auto &instr : process.instructions)
         {
             for (int j = 1; j < (int)instr.size(); j++)
             {
                 mainMemory[writeIndex++] = instr[j];
             }
         }
 
         readyQueue.push(process.main_memory_base);
         memoryIndex = process.instruction_base + process.max_memory_needed;
     }
 }
 
 /*************************************************
  * MAIN FUNCTION (from CS3113_Project2.cpp)
  *************************************************/
 int main(int argc, char **argv)
 {

       // 1. Open a file for output:
       std::ofstream outFile("out.txt");
       if (!outFile) 
       {
           std::cerr << "ERROR: Could not open out.txt for writing!\n";
           return 1; // bail out if the file can't be opened
       }

    // 2. Save the existing buffer (so we can restore it later):
    std::streambuf* oldCoutBuf = std::cout.rdbuf();

    // 3. Redirect std::cout to outFile’s buffer:
    std::cout.rdbuf(outFile.rdbuf());

     // define variables and newJobQueue and readyQueue
     int max_memory, num_processes;
     std::queue<PCB> newJobQueue;
     std::queue<int> readyQueue;
 
     // read in data: 
     //   max_memory, CPU_allocated, context_switch_time, num_processes
     std::cin >> max_memory >> CPU_allocated >> context_switch_time >> num_processes;
 
     // build linked list as one large free block
     MemoryBlock *memoryHead = new MemoryBlock(-1, 0, max_memory);
 
     // build a dynamic array for main memory
     int *main_memory = new int[max_memory];
     for (int i = 0; i < max_memory; i++)
         main_memory[i] = -1;
 
     // Initialize global arrays
     for (int i = 0; i < MAX_PID; i++)
     {
         param_off_sets[i]    = 0;
         process_start_times[i] = 0;
     }
 
     // Read all processes
     for (int i = 0; i < num_processes; i++)
     {
         PCB process;
         std::cin >> process.process_id >> process.max_memory_needed;
 
         int num_instructions;
         std::cin >> num_instructions;
 
         process.state           = STATE_NEW;
         process.memory_limit    = process.max_memory_needed;
         process.program_counter = 0;
         process.CPU_cycles_used = 0;
         process.register_value  = 0;
 
         // read instructions
         std::vector<std::vector<int>> instructions;
         instructions.reserve(num_instructions);
 
         for (int j = 0; j < num_instructions; j++)
         {
             int opcode;
             std::cin >> opcode;
             std::vector<int> inst;
             inst.push_back(opcode);
 
             int num_params = getParamCount(opcode);
             for (int k = 0; k < num_params; k++)
             {
                 int param;
                 std::cin >> param;
                 inst.push_back(param);
             }
             instructions.push_back(inst);
         }
         process.instructions = instructions;
         newJobQueue.push(process);
     }
 
     // For Project 3: We would do something like:
     //   while (!newJobQueue.empty()) {
     //       PCB p = newJobQueue.front();
     //       newJobQueue.pop();
     //       allocateMemory(memoryHead, p.process_id, p.max_memory_needed + 10); // +10 for metadata
     //       ...
     //   }
     // For now, let's just call the old loadJobsToMemory for demonstration
     loadJobsToMemory(newJobQueue, readyQueue, main_memory, max_memory);
 
     // DEBUG: show main memory contents
     for (int i = 0; i < max_memory; i++)
     {
         std::cout << i << " : " << main_memory[i] << "\n";
     }
 
     // CPU + IO loop
     while (!readyQueue.empty() || !IOWaitingQueue.empty())
     {
         if (!readyQueue.empty())
         {
             int startAddress = readyQueue.front();
             readyQueue.pop();
 
             executeCPU(startAddress, main_memory);
 
             // If we got a time-out, go back to ready
             if (timeout_occurred)
             {
                 readyQueue.push(startAddress);
                 timeout_occurred = false;
             }
         }
         else
         {
             // no process is ready => just skip time
             global_clock += context_switch_time;
         }
 
         // handle I/O completions
         checkIOWaitingQueue(readyQueue, main_memory);
     }
 
     global_clock += context_switch_time;
     std::cout << "Total CPU time used: " << global_clock << ".\n";
 
     // free memory
     delete[] main_memory;

     std::cout.rdbuf(oldCoutBuf);

     // If we want to print one last line to the console:
     std::cout << "All output was saved to out.txt!\n";
 
     // Clean up
     outFile.close();
 
     return 0;
 }
 