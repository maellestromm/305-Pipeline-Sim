# 305-Pipeline-Sim

# Currently implemented:

- Core header file proj.h with minimal definitions for Instruction struct, EventQueueNode struct, InstructionQueue class (adapted from prev. assignment's ElementQueue), EventQueue class (not functional, will need to be expanded as we define its scope and functionality), and Simulation class.
- Main() function in proj.cpp with input parameter handling, creation of new Simulation() object, does not yet start the simulation.
- InstructionQueue.cpp file with implementations for InstructionQueue functionality: input parsing, reads target lines of trace file (lines between [start_inst] and [start_inst + inst_count]) to initialize Instruction objects, which are then added to the InstructionQ. Prints list of Instructions in queue for debugging. Ensured leak-proof memory with Valgrind.
- Makefile, additional implementation .cpp file names should be added to the dependencies here so that they link correctly.

# Execute current functionality:

1. Run command "make extract"
2. Run command "make"
3. Run command "./proj compute_fp_1 1 100 1" to see printed InstructionQueue, list of first 100 instructions in compute_fp_1 trace file.

# Next steps:

1. Complete EventQueue implementation, looking to past homeworks for reference
2. Implement Simulation class, should have Process(EventType)() functions for each event type (as in previous assignments)
3. Implement core RunSimulation() function, schedule the first two (superscalar processor width = 2) IF events
4. Implement dependency handling, from initial research perhaps a std::unordered_map data type could be useful here?
5. Complete D=1 functionality with time tracking
6. Implement Running Replications / Experimental Design / Performance Metrics sections
7. Expand to D=2,3,4

# 4/8 Update:

- Wrote RunSimulation() in proj.cpp with a simple cycle-by-cycle pipeline (IF/ID/EX/MEM/WB), dependency checks, structural hazard checks, and a basic branch fetch stall.
- Added HasNext()/GetNext() in InstructionQueue so fetch can pull instructions.
- The sim prints total cycles, execution time (ms), and the instruction mix.
- Main() now runs the simulation.
- For now, only D=1 behavior is supported (D=2/3/4 still need the extra EX/MEM stages).

# 4/9 update

- Added multi-cycle EX behavior for FP instructions in D=2/4.
- Added multi-cycle MEM behavior for load instructions in D=3/4.
- Enforced in-order MEM retirement.
- Added more comments (especially to explain the hazard checks).

# 4/11 update

- Refactored pipeline stages into helper functions (ProcessIF/ID/EX/MEM/WB).
- Fixed PRIu64 printf formatting in InstructionQueue debug output.
- Added missing <fstream> include.



