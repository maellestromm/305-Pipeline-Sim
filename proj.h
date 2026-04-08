#include<stdio.h>
#include<time.h>
#include<math.h>
#include<stdlib.h>
#include<unistd.h>
#include<string>
#include <filesystem>
#include<cstdint>
#include<cinttypes>
#include<unordered_map>
#include<vector>
#include<deque>
#include<queue>
#include<fstream>
using namespace std;

/*
    Enumerations for interpretable reference to instruction types and pipeline stages:
*/

// Instruction type: A value between 1 and 5
enum InstType { 
    INT,        // Integer instruction: An instruction that uses the integer ALU
    FP,         // Floating point instruction: An instruction that uses the floating point unit
    BRANCH,     // Branch: An instruction that transfers control to the next instruction in the trace.
    LOAD,       // Load: An instruction that reads a value from memory.
    STORE       // Store: An instruction that writes a value to memory.
};
// Instruction processing stage: A value between 1 and 6 (including Done)
enum Stage { 
    IF,         // Instruction fetch (IF)
    ID,         // Instruction decode and read operands (ID)
    EX,         // Issue and Execute (EX)
    MEM,        // Memory read/write for load/store instructions (MEM)
    WB,         // Write back results to registers and Retire (WB)
    DONE        // Instruction completed
};

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
struct Instruction {
    // 1. Instruction program counter (PC)
    uint64_t pc;

    // 2. Instruction type: A value between 1 and 5
    InstType type;

    // 3. A list of PC values of instruction dependences
    vector<uint64_t> dependences;

    // Stored line number from trace (unique identification number)
    uint64_t number;
};

// Definition of a Queue Node in the Event Queue
struct EventQueueNode {
    double event_time;              // Event start time
    Stage stage;                    // Current stage of instruction processing
    Instruction* qnode;             // Pointer to corresponding instruction in the InstructionQueue
    struct EventQueueNode *next;    // Pointer to next event
};

/*
    Class definitions:
*/

// Class defining a queue of instructions read from the selected trace file
class InstructionQueue {
    public:

        /* TODO:
            Properly implement "initialize a new InstructionQueue by parsing through file"
        */
        // Constructor, create and return a new InstructionQueue
        InstructionQueue(string filename, int start_inst, int inst_count, int D_depth){
            
            // ? Initialize any variables tracked in this queue

            // Initialize the queue
            InitializeQueue(filename, start_inst, inst_count, D_depth);
        }
        ~InstructionQueue(){
            for (Instruction* inst : InstructionQ){
                delete inst;
            }
            InstructionQ.clear();
        }

        // Print all InstructionQueue nodes (for debugging)
        void PrintInstructionQ(){
            for (const auto& node : InstructionQ){
                printf("Inst #" PRIu64 ": \tPC = " PRIu64 ", \tInstType = %d, \tDependences = ",
                    node->number, node->pc, node->type);
                for (uint64_t d : node->dependences){
                    printf("" PRIu64 ", ", d);
                }
                printf("\n");
            }
        }

        // Check if there are more instructions in the queue
        bool HasNext(){
            return !InstructionQ.empty();
        }

        // Get the next instruction in the queue and remove it from the queue
        Instruction* GetNext(){
            if (InstructionQ.empty()){
                return nullptr;
            }
            Instruction* next_inst = InstructionQ.front();
            InstructionQ.pop_front();
            return next_inst;
        }

    private:
        // Initialize InstructionQueue by parsing through file line by line to create a queue of Instruction objects
        void InitializeQueue(string filename, int start_inst, int inst_count, int D_depth);

        deque<Instruction*> InstructionQ;

        // ?? Any variables tracked in this queue
};

// Event Queue for events that have been scheduled
class EventQueue {
    public:

        /* TODO:
            Implement EventQueue()
        */
        // Constructor, initialize a new queue of events
        //EventQueue();

    private:
        queue<EventQueueNode*> EventQ;
};


// Class defining, initializing, and running a Simple Processor Pipeline Simulator
class Simulation {
    public:

        // Constructor, initializes a new Simple Processor Pipeline Simulator using provided parameters
        Simulation(string filename_in, int start_inst_in, int inst_count_in, int D_depth_in){

            // Store parameters
            filename = filename_in;
            start_inst = start_inst_in;
            inst_count = inst_count_in;
            D_depth = D_depth_in;

            // Initialize underlying queues
            InstructionQ = new InstructionQueue(filename, start_inst, inst_count, D_depth);
            //EventQ = new EventQueue();

        };
        // Destructor, free allocated memory for underlying queues
        ~Simulation() {
			delete InstructionQ;
			//delete EventQ;
		};

        // Main simulator function
        void RunSimulation();

    private:
        // Underlying queues
        InstructionQueue* InstructionQ;
        [[maybe_unused]] EventQueue* EventQ;

        // Input parameters
        string filename;
        int start_inst;
        int inst_count;
        int D_depth;
};