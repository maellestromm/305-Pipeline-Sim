#include "proj.h"
#include <fstream>
#include <sstream>

// Initialize InstructionQueue by parsing through file line by line to create a queue of Instruction objects
void InstructionQueue::InitializeQueue(string filename, int start_inst, int inst_count, int D_depth)
{

    // Open trace file (filename exists, checked in proj.cpp: main())
    ifstream trace_file(filename);

    // Iterate by line until the line before the provided start_inst line is reached
    // Utilization of the fstream library implemented with help from: https://www.w3schools.com/cpp/cpp_files.asp
    int line_count = 0;
    string line;
    while (getline(trace_file, line) && line_count < start_inst - 1)
    {
        line_count++;
    }

    // Check: if start_inst line has not been reached, then start_inst > # lines in file
    if (line_count != start_inst - 1)
    {
        printf("Error initializing InstructionQueue: line %d exceeds total lines in file.\n", start_inst);
        trace_file.close();
        exit(1);
    }

    // Iterate by line until the provided # of lines = inst_count is reached
    // Reading CSV files implemented with help from: https://www.geeksforgeeks.org/cpp/csv-file-management-using-c/
    vector<uint64_t> row;
    string word, temp;
    while (line_count < start_inst + inst_count)
    {
        row.clear(); // clear data from last iteration

        // Read an entire row, split into words, and store each 'word' of column data to the row vector
        if (!getline(trace_file, line))
        { // Break from loop if no more lines remain in file
            printf("inst_count exceeded end of file.\n");
            trace_file.close();
            break;
        }
        stringstream s(line);

        // Assign first two column values (PC, InstType) to new Instruction object
        Instruction *inst = new Instruction;
        getline(s, word, ',');
        inst->pc = strtoull(word.c_str(), nullptr, 16); // pc = to_unsigned_int_64(column value 1)
        getline(s, word, ',');
        // Trace type is 1-5, enum is 0-4, so subtract 1
        inst->type = static_cast<InstType>(stoi(word) - 1); // type = to_enum(to_int(column value 2) - 1)

        // Read the remaining column values (PC dependences list)
        while (getline(s, word, ','))
        {
            row.push_back(strtoull(word.c_str(), nullptr, 16));
        }

        // Increment line counter
        line_count++;

        // Assign list of dependencies and line # to Instruction object
        inst->dependences = row;
        inst->number = line_count;

        // Add initialized instruction to the InstructionQueue
        InstructionQ.push_back(inst);
    }

    // Close file
    trace_file.close();
    // Set inst_count to actual number of instructions loaded if file was too short
    extern int actual_inst_count;
    actual_inst_count = static_cast<int>(InstructionQ.size());
    // Debugging: print contents of InstructionQ
    // PrintInstructionQ();
}