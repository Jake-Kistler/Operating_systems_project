//
// Created by jayki on 4/9/2025.
//

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

constexpr int MAX_SEGMENTS = 6;

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

std::unordered_map<std::string, int> state_encoding =
{
	{"NEW", 1},
	{"READY", 2},
	{"RUNNING", 3},
	{"TERMINATED", 4},
	{"IOWAITING", 5}
};

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
    int pcb_fields = 10; // the metadata excluding the segment table
    int instruction_count = 0;

    for(int i = 0; i < instructions.size(); i++)
    {
        instruction_count+= instructions[i].size(); // flatten the op codes and parameters since we have a nested vector this can be thought of linearizing a matrix to just a long column vector
    }

    out_bound_size = 1 + table_size + pcb_fields + instruction_count; // this is the total logical memory size
    int *logical_memory = new int[out_bound_size]; // create the new array of the out bound size

    int index = 0;
    logical_memory[index++] = table_size;

    // copy the segment table
    for(int i = 0; i < table_size; ++i)
    {
      logical_memory[index++] = process.segment_table[i];
    }

    // write the metadata fields
    logical_memory[index++] = process.process_id;
    logical_memory[index++] = state_encoding.at(process.state); // this takes the string and encodes it to the int values I have in "state_encoding" about line 150 or so
    logical_memory[index++] = process.program_counter;
    logical_memory[index++] = process.instruction_base;
    logical_memory[index++] = process.data_base;
    logical_memory[index++] = process.memory_limit;
    logical_memory[index++] = process.cpu_cycles_used;
    logical_memory[index++] = process.register_value;
    logical_memory[index++] = process.max_memory_needed;
    logical_memory[index++] = process.main_memory_base;

    // Flatten and copy the instructions over as well, a classic loop for a 2d array
    for(int i = 0; i < instructions.size(); ++i)
    {
      for(int j = 0; j < instructions[i].size(); ++j)
      {
        logical_memory[index++] = instructions[i][j];
      }
    }

    return logical_memory;
}

void copy_process_to_memory(int *logical_memory, int total_size, const PCB &pcb, int * main_memory)
{
    int logical_index = 0;

    // walk through each segment in the PCB
    for(int i = 0; i < pcb.number_of_segments; i++)
    {
      int start = pcb.segment_table[2 * i]; // the physical starting point
      int size = pcb.segment_table[2 * i + 1]; // size of the segment

      // copy a chunk of logical memory into the current segment
      for(int j = 0; j < size && logical_index < total_size; j++)
      {
        main_memory[start + j] = logical_memory[logical_index++];
      }
    }

    if(logical_index < total_size)
    {
        std::cout << "Error: not enough space in allocated segments to hold process.\n";
    }
}

// ------------------ MAIN TEST -------------------
int main()
{
  constexpr int MEMORY_SIZE = 500;
  int main_memory[MEMORY_SIZE];

  for(int i = 0; i < MEMORY_SIZE; i++)
    main_memory[i] = -1;

  PCB pcb;
  pcb.process_id = 1;
  pcb.state = "READY";
  pcb.program_counter = 0;
  pcb.instruction_base = 0;
  pcb.data_base = 0;
  pcb.memory_limit = 100;
  pcb.cpu_cycles_used = 0;
  pcb.register_value = 0;
  pcb.max_memory_needed = 100;
  pcb.main_memory_base = 100;

  pcb.number_of_segments = 2;
  pcb.segment_table_size = 4;

  // Simulate the segments sizes
  pcb.segment_table[0] = 100; // segment 0 start
  pcb.segment_table[1] = 50; // segment 0 size

  pcb.segment_table[2] = 300; // segemnt 1 start
  pcb.segment_table[3] = 50; // segment 1 size

  // mock instruuctions compute and print
  std::vector<std::vector<int>> instructions = {
    {1,5,3},
    {2,4}
  };

  int logical_size;
  int *logical_memory = build_logical_memory_array(pcb, instructions, logical_size);

  copy_process_to_memory(logical_memory, logical_size, pcb, main_memory);

  std::cout << "Memory copied. Verifying this translation...\n";

  // attempt to translate a few logical address
  for(int logical_address = 0; logical_address < 70; ++logical_address)
  {
    if(logical_address == 50)
      std::cout << "------- Entering segment 1 ---- " << std::endl;


    int physical_address = translate_logical_to_physical(logical_address, pcb);
    if(physical_address != -1)
      std::cout << "Main memory[" << physical_address << "] = " << main_memory[physical_address] << " (from logical " << logical_address << ")" << std::endl << std::endl;
  }
  delete[] logical_memory;
  return 0;
}
