#include <iostream>
#include <tuple>
#include <queue>
#include <vector>
#include <string>

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


struct memory_block
{
    int process_id; // -1 if free otherwise holds the process ID
    int start_address; // starting memory adress
    int block_size; // size of the block
    memory_block *next; // pointer to the next block in the linked list

    // constructor for this
    memory_block(int id, int start, int size)
    {
        process_id = id;
        start_address = start;
        block_size = size;
        next = nullptr;
    }
};











// State codes constexp is a form of constants that are type safe (found them here: https://en.cppreference.com/w/cpp/language/constexpr)
constexpr int STATE_NEW = 1;
constexpr int STATE_READY = 2;
constexpr int STATE_RUNNING = 3;
constexpr int STATE_TERMINATED = 4;
constexpr int STATE_IOWAITING = 5;

// PCB struct from the project
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
    // Each instruction is [opcode, param1, param2, ...]
    std::vector<std::vector<int>> instructions;
};

// This structure is a static vector that we will use to compare the opcode and parameters with.
static std::vector<std::vector<int>> opcodeParamsVector = {
    {1, 2}, // Compute => 2 params
    {2, 1}, // Print   => 1 param
    {3, 2}, // Store   => 2 params
    {4, 1}, // Load    => 1 param
};

static const int MAX_PID = 10000;
static int param_off_sets[MAX_PID];
static int process_start_times[MAX_PID];

int global_clock = 0;
bool timeout_occurred = false;
int context_switch_time, CPU_allocated;


/*
* This queue is a made of a tuple made of the PCB, int (start address) int, (wait time) and int (Time it entered the I/O queue
* each entry on the queue is one of these toupls which makes it easy to keep track across the life of the program )
*/
std::queue<std::tuple<PCB, int, int, int>> IOWaitingQueue;

std::string stateToString(int state_code);

void loadJobsToMemory(std::queue<PCB> &newJobQueue, std::queue<int> &readyQueue, int *mainMemory, int maxMemory);

int getParamCount(int opcode);

void executeCPU(int startAddress, int *mainMemory);

void checkIOWaitingQueue(std::queue<int> &readyQueue, int *mainMemory);

int main(int argc, char **argv)
{
    // define variables and newJobQueue and readyQueue
    int max_memory, num_processes;
    std::queue<PCB> newJobQueue;
    std::queue<int> readyQueue;

    // read in data
    std::cin >> max_memory >> CPU_allocated >> context_switch_time >> num_processes;

    // build a dynamic array and fill it with -1, changed this because I've come to realize how much hand holding modern programming languages do. Thanks MIPS for opening my eyes
    int *main_memory = new int[max_memory];
    for (int i = 0; i < max_memory; i++)
        main_memory[i] = -1;

    for (int i = 0; i < MAX_PID; i++)
    {
        param_off_sets[i] = 0;
        process_start_times[i] = 0;
    }

    // Read processes
    for (int i = 0; i < num_processes; i++)
    {
        PCB process;
        std::cin >> process.process_id >> process.max_memory_needed;

        int num_instructions;
        std::cin >> num_instructions;

        process.state = STATE_NEW;
        process.memory_limit = process.max_memory_needed;
        process.program_counter = 0;
        process.CPU_cycles_used = 0;
        process.register_value = 0;

        std::vector<std::vector<int>> instructions;
        instructions.reserve(num_instructions); // reverse so we get the first instruction read in to be first

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

    loadJobsToMemory(newJobQueue, readyQueue, main_memory, max_memory);

    // Debug: show contents of mainMemory
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

            if (timeout_occurred)
            {
                readyQueue.push(startAddress);
                timeout_occurred = false;
            }
        }
        else
        {
            global_clock += context_switch_time;
        }

        checkIOWaitingQueue(readyQueue, main_memory);
    }

    global_clock += context_switch_time;
    std::cout << "Total CPU time used: " << global_clock << "." << "\n";

    delete[] main_memory; // free memory since we used a dynamically allocated array
    return 0;
}

// Convert numeric state code to string
std::string stateToString(int state_code)
{
    switch (state_code)
    {
    case 1:
        return "NEW";
    case 2:
        return "READY";
    case 3:
        return "RUNNING";
    case 4:
        return "TERMINATED";
    case 5:
        return "IOWAITING";
    }
    return "UNKNOWN";
}

void loadJobsToMemory(std::queue<PCB> &newJobQueue, std::queue<int> &readyQueue, int *mainMemory, int maxMemory)
{
    int memoryIndex = 0;
    while (!newJobQueue.empty())
    {
        PCB process = newJobQueue.front();
        newJobQueue.pop();

        if (memoryIndex + process.max_memory_needed > maxMemory)
        {
            std::cout << "Not enough memory to load Process " << process.process_id << "\n";
            continue;
        }

        process.main_memory_base = memoryIndex;
        process.instruction_base = memoryIndex + 10;
        process.data_base = process.instruction_base + (int)process.instructions.size();

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

int getParamCount(int opcode)
{
    for (auto &opinfo : opcodeParamsVector)
    {
        if (opinfo[0] == opcode)
            return opinfo[1];
    }
    return 0;
}

void executeCPU(int startAddress, int *mainMemory)
{
    PCB process;
    process.process_id = mainMemory[startAddress + 0];
    process.state = mainMemory[startAddress + 1];
    process.program_counter = mainMemory[startAddress + 2];
    process.instruction_base = mainMemory[startAddress + 3];
    process.data_base = mainMemory[startAddress + 4];
    process.memory_limit = mainMemory[startAddress + 5];
    process.CPU_cycles_used = mainMemory[startAddress + 6];
    process.register_value = mainMemory[startAddress + 7];
    process.max_memory_needed = mainMemory[startAddress + 8];
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
            int cycles = mainMemory[process.data_base + paramOffset + 1];

            std::cout << "compute\n";

            process.CPU_cycles_used += cycles;
            mainMemory[startAddress + 6] = process.CPU_cycles_used;

            cpu_cycles_this_run += cycles;
            global_clock += cycles;

            break;
        }
        case 2: // print => IO
        {
            int cycles = mainMemory[process.data_base + paramOffset];
            std::cout << "Process " << pid << " issued an IOInterrupt and moved to the IOWaitingQueue.\n";

            // use std::make_tuple instead of brace init, the gradescope compiler did not like what I had here previously since it's a newish feature of cpp. I believe this is now ad older way of doing this previously I had this be a brace defined statement 
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
            break;
        }

        process.program_counter++;
        mainMemory[startAddress + 2] = process.program_counter;

        paramOffset += getParamCount(opcode);
        param_off_sets[pid] = paramOffset;

        if (cpu_cycles_this_run >= CPU_allocated && process.program_counter < process.data_base)
        {
            std::cout << "Process " << pid << " has a TimeOUT interrupt and is moved to the ReadyQueue.\n";
            process.state = STATE_READY;

            mainMemory[startAddress + 1] = process.state;
            timeout_occurred = true;
            return;
        }
    }

    process.program_counter = process.instruction_base - 1;
    mainMemory[startAddress + 2] = process.program_counter;

    process.state = STATE_TERMINATED;
    mainMemory[startAddress + 1] = process.state;

    int totalExecutionTime = global_clock - process_start_times[pid];

    std::cout << "Process ID: " << pid << "\n";
    std::cout << "State: " << stateToString(process.state) << "\n";
    std::cout << "Program Counter: " << process.program_counter << "\n";
    std::cout << "Instruction Base: " << process.instruction_base << "\n";
    std::cout << "Data Base: " << process.data_base << "\n";
    std::cout << "Memory Limit: " << process.memory_limit << "\n";
    std::cout << "CPU Cycles Used: " << process.CPU_cycles_used << "\n";
    std::cout << "Register Value: " << process.register_value << "\n";
    std::cout << "Max Memory Needed: " << process.max_memory_needed << "\n";
    std::cout << "Main Memory Base: " << process.main_memory_base << "\n";
    std::cout << "Total CPU Cycles Consumed: " << totalExecutionTime << "\n";

    std::cout << "Process " << pid
              << " terminated. Entered running state at: "
              << process_start_times[pid]
              << ". Terminated at: "
              << global_clock
              << ". Total Execution Time: "
              << totalExecutionTime
              << "." << "\n";
}

void checkIOWaitingQueue(std::queue<int> &readyQueue, int *mainMemory)
{
    int size = (int)IOWaitingQueue.size();
    for (int i = 0; i < size; i++)
    {
        
        std::tuple<PCB, int, int, int> frontItem = IOWaitingQueue.front();
        IOWaitingQueue.pop();

        // Grab the items in the queue
        PCB process = std::get<0>(frontItem);
        int startAddress = std::get<1>(frontItem);
        int waitTime = std::get<2>(frontItem);
        int timeEnteredIO = std::get<3>(frontItem);

        int pid = process.process_id;

        if (global_clock - timeEnteredIO >= waitTime)
        {
            int param_off_set = param_off_sets[pid];
            int cycles = mainMemory[process.data_base + param_off_set];

            std::cout << "print\n";
            process.CPU_cycles_used += cycles;
            mainMemory[startAddress + 6] = process.CPU_cycles_used;

            process.program_counter++;
            mainMemory[startAddress + 2] = process.program_counter;

            param_off_set += getParamCount(2); // print => 1 param
            param_off_sets[pid] = param_off_set;

            process.state = STATE_READY;
            mainMemory[startAddress + 1] = process.state;

            std::cout << "Process " << pid << " completed I/O and is moved to the ReadyQueue.\n";
            readyQueue.push(startAddress);
        }
        else
        {
            // Reinsert using std::make_tuple
            IOWaitingQueue.push(std::make_tuple(process, startAddress, waitTime, timeEnteredIO));
        }
    }
}
