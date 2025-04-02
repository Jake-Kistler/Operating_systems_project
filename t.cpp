#include <iostream>
#include <tuple>
#include <queue>
#include <vector>
#include <string>

// =====================
// State definitions
// =====================
constexpr int STATE_NEW = 1;
constexpr int STATE_READY = 2;
constexpr int STATE_RUNNING = 3;
constexpr int STATE_TERMINATED = 4;
constexpr int STATE_IOWAITING = 5;

// =====================
// PCB and MemoryBlock
// =====================
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

struct MemoryBlock
{
    int process_id;   // -1 if free
    int start_address;
    int size;
    MemoryBlock* next;

    MemoryBlock(int pid, int sAddress, int sz, MemoryBlock* nxt)
        : process_id(pid), start_address(sAddress), size(sz), next(nxt) {}

    MemoryBlock()
        : process_id(-1), start_address(0), size(0), next(nullptr) {}
};

// =====================
// Constants, Globals
// =====================
static const int MAX_PID = 10000;
static int param_off_sets[MAX_PID];      // used instead of Francco's paramOffsets
static int process_start_times[MAX_PID]; // used instead of Francco's processStartTimes

int global_clock = 0;
bool timeout_occurred = false;

int context_switch_time, CPU_allocated;
int *main_memory = nullptr;      // We'll allocate this at runtime
int max_memory   = 0;
int num_processes= 0;

// One big free block initially
MemoryBlock* memory_head = nullptr;

// =====================
// I/O Waiting Queue
//   (same as CS3113)
// =====================
std::queue<std::tuple<PCB, int, int, int>> IOWaitingQueue;

// =====================
// From CS3113: For #params
// of each opcode
// =====================
static std::vector<std::vector<int>> opcodeParamsVector = {
    {1, 2}, // Compute => 2 params
    {2, 1}, // Print   => 1 param
    {3, 2}, // Store   => 2 params
    {4, 1}, // Load    => 1 param
};

int getParamCount(int opcode)
{
    for (auto &op : opcodeParamsVector) {
        if (op[0] == opcode) return op[1];
    }
    return 0;
}

// =====================
// stateToString
//   (same as CS3113)
// =====================
std::string stateToString(int state_code)
{
    switch (state_code)
    {
    case STATE_NEW:       return "NEW";
    case STATE_READY:     return "READY";
    case STATE_RUNNING:   return "RUNNING";
    case STATE_TERMINATED:return "TERMINATED";
    case STATE_IOWAITING: return "IOWAITING";
    default:              return "UNKNOWN";
    }
}

// =====================================================================
// MERGED VERSIONS OF FRANCCO’S allocateMemory, freeMemory, coalesceMemory
// (Adjusting them to reference memory_head as a global, minimal changes.)
// =====================================================================

// (Merged) from Francco.cpp
int allocateMemory(MemoryBlock*& memoryHead, int processID, int size)
{
    MemoryBlock* current = memoryHead;
    MemoryBlock* prev    = nullptr;

    while (current)
    {
        // If block is free and big enough
        if (current->process_id == -1 && current->size >= size)
        {
            int allocatedAddress = current->start_address;

            // exact fit
            if (current->size == size)
            {
                current->process_id = processID;
            }
            else
            {
                // create a new block for the allocated portion
                MemoryBlock* newBlock = new MemoryBlock(processID,
                                                        current->start_address,
                                                        size,
                                                        nullptr);
                // We need a constructor, or set fields, because we have CS3113 struct
                newBlock->process_id    = processID;
                newBlock->start_address = current->start_address;
                newBlock->size          = size;
                newBlock->next          = current;

                if (prev) {
                    prev->next = newBlock;
                } else {
                    memoryHead = newBlock;
                }

                current->start_address += size;
                current->size          -= size;
                return allocatedAddress;
            }
            return allocatedAddress;
        }
        prev    = current;
        current = current->next;
    }
    return -1; // No free block
}

// (Merged) from Francco.cpp
void freeMemory(MemoryBlock*& memoryHead, int *mainMemory, int processID)
{
    MemoryBlock* current = memoryHead;
    while (current)
    {
        if (current->process_id == processID)
        {
            // Mark memory as free
            for (int i = current->start_address; i < current->start_address + current->size; i++)
            {
                mainMemory[i] = -1;
            }
            current->process_id = -1;
            return; // done
        }
        current = current->next;
    }
}

// (Merged) from Francco.cpp
void coalesceMemory(MemoryBlock*& memoryHead)
{
    MemoryBlock* current = memoryHead;
    while (current && current->next)
    {
        MemoryBlock* nxt = current->next;
        if (current->process_id == -1 && nxt->process_id == -1)
        {
            current->size += nxt->size;
            current->next  = nxt->next;
            delete nxt;
            continue; // re-check current
        }
        current = current->next;
    }
}

// =====================================================================
// MERGED loadJobsToMemory (mostly from Francco.cpp), but adapted to
// read instructions from process.instructions instead of a global map.
// =====================================================================
void loadJobsToMemory(std::queue<PCB> &newJobQueue,
                      std::queue<int> &readyQueue,
                      int *mainMemory,
                      MemoryBlock *&memoryHead)
{
    int newJobQueueSize = (int)newJobQueue.size();
    std::queue<PCB> tempQueue; // for jobs that can’t be loaded now

    for (int i = 0; i < newJobQueueSize; i++)
    {
        PCB process = newJobQueue.front();
        newJobQueue.pop();

        int totalMemoryNeeded = process.max_memory_needed + 10; // 10 for PCB “metadata”
        int allocAddress = allocateMemory(memoryHead, process.process_id, totalMemoryNeeded);

        bool coalescedForThisProcess = false;
        if (allocAddress == -1)
        {
            std::cout << "Insufficient memory for Process " << process.process_id
                      << ". Attempting memory coalescing." << std::endl;
            coalesceMemory(memoryHead);

            allocAddress = allocateMemory(memoryHead, process.process_id, totalMemoryNeeded);
            coalescedForThisProcess = (allocAddress != -1);

            if (allocAddress == -1)
            {
                std::cout << "Process " << process.process_id
                          << " waiting in NewJobQueue due to insufficient memory." << std::endl;
                // push process + all the rest onto tempQueue
                tempQueue.push(process);
                for (int j = i+1; j < newJobQueueSize; j++)
                {
                    tempQueue.push(newJobQueue.front());
                    newJobQueue.pop();
                }
                break; // done this pass
            }
        }

        if (allocAddress != -1)
        {
            if (coalescedForThisProcess)
            {
                std::cout << "Memory coalesced. Process "
                          << process.process_id
                          << " can now be loaded." << std::endl;
            }

            // Fill in memory-base addresses
            process.main_memory_base = allocAddress;
            process.instruction_base = allocAddress + 10; // after the PCB
            // instructions are laid out first (opcodes), then parameters
            process.data_base        = process.instruction_base
                                       + (int)process.instructions.size();

            // Write PCB metadata
            mainMemory[allocAddress + 0] = process.process_id;
            mainMemory[allocAddress + 1] = process.state;
            mainMemory[allocAddress + 2] = process.program_counter;
            mainMemory[allocAddress + 3] = process.instruction_base;
            mainMemory[allocAddress + 4] = process.data_base;
            mainMemory[allocAddress + 5] = process.memory_limit;
            mainMemory[allocAddress + 6] = process.CPU_cycles_used;
            mainMemory[allocAddress + 7] = process.register_value;
            mainMemory[allocAddress + 8] = process.max_memory_needed;
            mainMemory[allocAddress + 9] = process.main_memory_base;

            // Store opcodes
            int writeIndex = process.instruction_base;
            for (auto &instr : process.instructions) {
                mainMemory[writeIndex++] = instr[0];
            }
            // then store parameters
            for (auto &instr : process.instructions) {
                for (int idx = 1; idx < (int)instr.size(); idx++) {
                    mainMemory[writeIndex++] = instr[idx];
                }
            }

            std::cout << "Process " << process.process_id
                      << " loaded into memory at address "
                      << allocAddress << " with size "
                      << totalMemoryNeeded << "." << std::endl;

            // push to readyQueue
            readyQueue.push(process.main_memory_base);
        }
    }

    // put back failed jobs
    while (!tempQueue.empty())
    {
        newJobQueue.push(tempQueue.front());
        tempQueue.pop();
    }
}

// =====================================================================
// MERGED executeCPU (mostly from Francco.cpp), adapted to
//   - use param_off_sets[pid]
//   - preserve minimal changes to your structure
// =====================================================================
void executeCPU(int startAddress,
                int *mainMemory,
                MemoryBlock *&memoryHead,
                std::queue<PCB>& newJobQueue,
                std::queue<int>& readyQueue)
{
    // Rebuild the PCB from memory
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

    int pid                = process.process_id;
    int cpuCyclesThisRun   = 0;

    // context switch overhead
    global_clock += context_switch_time;

    // If first time
    if (process.program_counter == 0)
    {
        process.program_counter   = process.instruction_base;
        param_off_sets[pid]       = 0;
        process_start_times[pid]  = global_clock;
    }

    // Mark running
    process.state                  = STATE_RUNNING;
    mainMemory[startAddress + 1]   = process.state;
    mainMemory[startAddress + 2]   = process.program_counter;
    std::cout << "Process " << pid << " has moved to Running." << std::endl;

    int &paramOffset = param_off_sets[pid];

    // === Main CPU loop ===
    while (process.program_counter < process.data_base &&
           cpuCyclesThisRun < CPU_allocated)
    {
        int opcode = mainMemory[process.program_counter];

        switch (opcode)
        {
        case 1: // compute
        {
            int iterations = mainMemory[process.data_base + paramOffset];
            int cycles     = mainMemory[process.data_base + paramOffset + 1];
            std::cout << "compute" << std::endl;

            process.CPU_cycles_used += cycles;
            mainMemory[startAddress + 6] = process.CPU_cycles_used;

            cpuCyclesThisRun += cycles;
            global_clock     += cycles;
            break;
        }
        case 2: // print => IO
        {
            int cycles = mainMemory[process.data_base + paramOffset];
            std::cout << "Process " << pid
                      << " issued an IOInterrupt and moved to the IOWaitingQueue."
                      << std::endl;

            IOWaitingQueue.push({process, startAddress, cycles, global_clock});

            process.state = STATE_IOWAITING;
            mainMemory[startAddress + 1] = process.state;
            return; // done for now
        }
        case 3: // store
        {
            int value   = mainMemory[process.data_base + paramOffset];
            int address = mainMemory[process.data_base + paramOffset + 1];

            process.register_value = value;
            mainMemory[startAddress + 7] = process.register_value;

            if (address < process.memory_limit)
            {
                mainMemory[process.main_memory_base + address] = process.register_value;
                std::cout << "stored" << std::endl;
            }
            else
            {
                std::cout << "store error!" << std::endl;
            }

            process.CPU_cycles_used++;
            mainMemory[startAddress + 6] = process.CPU_cycles_used;
            cpuCyclesThisRun++;
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
                std::cout << "loaded" << std::endl;
            }
            else
            {
                std::cout << "load error!" << std::endl;
            }
            process.CPU_cycles_used++;
            mainMemory[startAddress + 6] = process.CPU_cycles_used;
            cpuCyclesThisRun++;
            global_clock++;
            break;
        }
        default:
            // no-op or unknown
            break;
        }

        // increment PC
        process.program_counter++;
        mainMemory[startAddress + 2] = process.program_counter;

        // move paramOffset
        paramOffset += getParamCount(opcode);

        // check time-out
        if (cpuCyclesThisRun >= CPU_allocated &&
            process.program_counter < process.data_base)
        {
            std::cout << "Process " << pid
                      << " has a TimeOUT interrupt and is moved to the ReadyQueue.\n";
            process.state = STATE_READY;
            mainMemory[startAddress + 1] = process.state;

            timeout_occurred = true;
            return;
        }
    }

    // If done instructions
    process.program_counter = process.instruction_base - 1;
    mainMemory[startAddress + 2] = process.program_counter;

    // Terminated
    process.state = STATE_TERMINATED;
    mainMemory[startAddress + 1] = process.state;

    int totalExecTime = global_clock - process_start_times[pid];

    // Print final PCB info
    std::cout << "Process ID: " << pid << std::endl
              << "State: " << stateToString(process.state) << std::endl
              << "Program Counter: " << process.program_counter << std::endl
              << "Instruction Base: " << process.instruction_base << std::endl
              << "Data Base: " << process.data_base << std::endl
              << "Memory Limit: " << process.memory_limit << std::endl
              << "CPU Cycles Used: " << process.CPU_cycles_used << std::endl
              << "Register Value: " << process.register_value << std::endl
              << "Max Memory Needed: " << process.max_memory_needed << std::endl
              << "Main Memory Base: " << process.main_memory_base << std::endl
              << "Total CPU Cycles Consumed: " << totalExecTime << std::endl;

    std::cout << "Process " << pid
              << " terminated. Entered running state at: "
              << process_start_times[pid]
              << ". Terminated at: "
              << global_clock
              << ". Total Execution Time: "
              << totalExecTime
              << "." << std::endl;

    // free memory
    freeMemory(memoryHead, mainMemory, pid);
    std::cout << "Process " << pid << " terminated and released memory from "
              << startAddress << " to "
              << (startAddress + process.max_memory_needed + 10 - 1)
              << "." << std::endl;
}

// =====================
// checkIOWaitingQueue
//   (Keep from CS3113)
// =====================
void checkIOWaitingQueue(std::queue<int> &readyQueue, int *mainMemory)
{
    int size = (int)IOWaitingQueue.size();
    for (int i = 0; i < size; i++)
    {
        auto frontItem   = IOWaitingQueue.front();
        IOWaitingQueue.pop();

        PCB  process     = std::get<0>(frontItem);
        int  startAddr   = std::get<1>(frontItem);
        int  waitTime    = std::get<2>(frontItem);
        int  timeEntered = std::get<3>(frontItem);

        int pid = process.process_id;

        if (global_clock - timeEntered >= waitTime)
        {
            // param offset
            int paramOffset = param_off_sets[pid];
            int cycles      = mainMemory[process.data_base + paramOffset];

            std::cout << "print" << std::endl;
            process.CPU_cycles_used += cycles;
            mainMemory[startAddr + 6] = process.CPU_cycles_used;

            process.program_counter++;
            mainMemory[startAddr + 2] = process.program_counter;

            // opcode=2 => 1 param
            paramOffset += getParamCount(2);
            param_off_sets[pid] = paramOffset;

            process.state = STATE_READY;
            mainMemory[startAddr + 1] = process.state;

            std::cout << "Process " << pid
                      << " completed I/O and is moved to the ReadyQueue."
                      << std::endl;

            readyQueue.push(startAddr);
        }
        else
        {
            IOWaitingQueue.push(frontItem); // not done
        }
    }
}

// =====================
// main()
//   (Mostly from CS3113)
// =====================
int main(int argc, char** argv)
{
    std::queue<PCB> newJobQueue;
    std::queue<int> readyQueue;

    std::cin >> max_memory >> CPU_allocated
             >> context_switch_time >> num_processes;

    main_memory = new int[max_memory];
    for (int i = 0; i < max_memory; i++) {
        main_memory[i] = -1;
    }

    // Create a single big free block
    memory_head = new MemoryBlock;
    memory_head->process_id    = -1;
    memory_head->start_address = 0;
    memory_head->size          = max_memory;
    memory_head->next          = nullptr;

    // initialize arrays
    for (int i = 0; i < MAX_PID; i++) {
        param_off_sets[i]      = 0;
        process_start_times[i] = 0;
    }

    // Read processes
    for (int i = 0; i < num_processes; i++)
    {
        PCB process;
        std::cin >> process.process_id
                 >> process.max_memory_needed;

        int num_instructions;
        std::cin >> num_instructions;

        process.state           = STATE_NEW;
        process.memory_limit    = process.max_memory_needed;
        process.program_counter = 0;
        process.CPU_cycles_used = 0;
        process.register_value  = 0;

        for (int j = 0; j < num_instructions; j++)
        {
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

    // First load
    loadJobsToMemory(newJobQueue, readyQueue, main_memory, memory_head);

    // Debug: print memory
    for (int i = 0; i < max_memory; i++) {
        std::cout << i << " : " << main_memory[i] << "\n";
    }

    // CPU + IO loop
    while (!readyQueue.empty() || !IOWaitingQueue.empty())
    {
        if (!readyQueue.empty())
        {
            int startAddress = readyQueue.front();
            readyQueue.pop();

            executeCPU(startAddress, main_memory, memory_head, newJobQueue, readyQueue);

            if (timeout_occurred)
            {
                readyQueue.push(startAddress);
                timeout_occurred = false;
            }
        }
        else
        {
            // No ready processes, but maybe IO is ongoing
            global_clock += context_switch_time;
        }

        // Check IO completion
        checkIOWaitingQueue(readyQueue, main_memory);
    }

    global_clock += context_switch_time;
    std::cout << "Total CPU time used: " << global_clock << ".\n";

    delete[] main_memory;
    return 0;
}
