#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <filesystem>
#include <cstdint>
#include <cinttypes>
#include <unordered_map>
#include <vector>
#include <deque>
using namespace std;

/*
    Enumerations for interpretable reference to instruction types and pipeline stages:
*/

// Instruction type: A value between 1 and 5
enum InstType
{
    INT,    // Integer instruction: An instruction that uses the integer ALU
    FP,     // Floating point instruction: An instruction that uses the floating point unit
    BRANCH, // Branch: An instruction that transfers control to the next instruction in the trace.
    LOAD,   // Load: An instruction that reads a value from memory.
    STORE   // Store: An instruction that writes a value to memory.
};
// Instruction processing stage: A value between 1 and 6 (including Done)
enum Stage
{
    IF,  // Instruction fetch (IF)
    ID,  // Instruction decode and read operands (ID)
    EX,  // Issue and Execute (EX)
    MEM, // Memory read/write for load/store instructions (MEM)
    WB,  // Write back results to registers and Retire (WB)
    DONE // Instruction completed
};

// Global variable to hold the actual number of instructions loaded
extern int actual_inst_count;

/*
    Data structure definitions:
*/

// Each line of the trace represents an instruction with comma separated values representing the following:
// 1. Instruction program counter (PC): A hexadecimal value representing the instruction address.
// 2. Instruction type: A value between 1 and 5
// 3. A list of PC values for instructions that the current instruction depends on.
//    - Some instructions don’t have any dependences, so this list will be empty.
//    - Other instructions depend on 1-4 other instructions.
//    Note that PC values correspond to static instructions, and the trace could have multiple dynamic
//    instructions with the same PC value. For dependences, an instruction is dependent on the last instance
//    of the dynamic instruction with that PC value.
struct Instruction
{
    // 1. Instruction program counter (PC)
    uint64_t pc;

    // 2. Instruction type: A value between 1 and 5
    InstType type;

    // 3. A list of PC values of instruction dependences
    vector<uint64_t> dependences;

    // Stored line number from trace (unique identification number)
    uint64_t number;
};

/*
    Class definitions:
*/

// Class defining a queue of instructions read from the selected trace file
class InstructionQueue
{
public:
    // Constructor, create and return a new InstructionQueue
    InstructionQueue(string filename, int start_inst, int inst_count, int D_depth)
    {
        // Initialize the queue
        InitializeQueue(filename, start_inst, inst_count, D_depth);
    }
    ~InstructionQueue()
    {
        for (Instruction *inst : InstructionQ)
        {
            delete inst;
        }
        InstructionQ.clear();
    }

    // Print all InstructionQueue nodes (for debugging)
    void PrintInstructionQ()
    {
        for (const auto &node : InstructionQ)
        {
            printf("Inst #%" PRIu64 ": \tPC = %" PRIu64 ", \tInstType = %d, \tDependences = ",
                   node->number, node->pc, node->type);
            for (uint64_t d : node->dependences)
            {
                printf("%" PRIu64 ", ", d);
            }
            printf("\n");
        }
    }

    // Check if there are more instructions in the queue
    bool HasNext()
    {
        return !InstructionQ.empty();
    }

    // Get the next instruction in the queue and remove it from the queue
    Instruction *GetNext()
    {
        if (InstructionQ.empty())
        {
            return nullptr;
        }
        Instruction *next_inst = InstructionQ.front();
        InstructionQ.pop_front();
        return next_inst;
    }

private:
    // Initialize InstructionQueue by parsing through file line by line to create a queue of Instruction objects
    void InitializeQueue(string filename, int start_inst, int inst_count, int D_depth);

    deque<Instruction *> InstructionQ;
};

// Class defining, initializing, and running a Simple Processor Pipeline Simulator
class Simulation
{
public:
    // Constructor, initializes a new Simple Processor Pipeline Simulator using provided parameters
    Simulation(string filename_in, int start_inst_in, int inst_count_in, int D_depth_in)
    {

        // Store parameters
        filename = filename_in;
        start_inst = start_inst_in;
        inst_count = inst_count_in;
        D_depth = D_depth_in;

        // Initialize underlying queues
        InstructionQ = new InstructionQueue(filename, start_inst, inst_count, D_depth);

        // Simulate only the instructions that were actually loaded from the trace
        if (actual_inst_count < inst_count)
        {
            inst_count = actual_inst_count;
        }
    };
    // Destructor, free allocated memory for underlying queues
    ~Simulation()
    {
        delete InstructionQ;
    };

    // Main simulator function
    void RunSimulation();

    // Move instructions from IF to ID stage if possible, and fetch new instructions into IF stage if not stalled
    void ProcessIF(Instruction *(&IF)[2], Instruction *(&ID)[2], InstructionQueue *InstructionQ, bool &stall_fetch, bool &stall_fetch_next);

    // Move instructions from ID to EX stage if possible, and update stall_fetch_next for the next cycle based on dependences
    void ProcessID(Instruction *(&ID)[2], Instruction *(&EX)[2], int (&EX_cycles_left)[2], int cycle, int D_depth, unordered_map<uint64_t, uint64_t> &last_done_cycle, bool &stall_fetch_next);

    // Move instructions from EX to MEM stage if possible, and update EX_cycles_left and MEM_cycles_left for the next cycle based on D and instruction type
    void ProcessEX(Instruction *(&EX)[2], Instruction *(&MEM)[2], int (&EX_cycles_left)[2], int (&MEM_cycles_left)[2], int cycle, int D_depth, unordered_map<uint64_t, uint64_t> &last_done_cycle);

    // Move instructions from MEM to WB stage if possible
    void ProcessMEM(Instruction *(&MEM)[2], Instruction *(&WB)[2], int (&MEM_cycles_left)[2], unordered_map<uint64_t, uint64_t> &last_done_cycle, int cycle);

    // Move instructions from WB to Done stage, and update retired_inst and histogram counts for the next cycle
    void ProcessWB(Instruction *(&WB)[2], int &retired_inst, int (&histogram)[5]);

private:
    // Keep partially-filled pipeline stages left-aligned so slot 0 is always used first.
    void CompactStage(Instruction *(&stage)[2]);
    void CompactStageWithCycles(Instruction *(&stage)[2], int (&cycles_left)[2]);

    // Underlying queues
    InstructionQueue *InstructionQ;

    // Input parameters
    string filename;
    int start_inst;
    int inst_count;
    int D_depth;
};
