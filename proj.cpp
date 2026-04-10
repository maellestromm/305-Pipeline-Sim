#include "proj.h"

// Overall flow:
// 1. Read the trace into the instruction queue
// 2. Run cycle by cycle until all instructions finish
//    - WB: retire + histogram update
//    - MEM: move to WB
//    - EX: move to MEM (structural hazard: load/store MEM conflicts)
//    - ID: move to EX (data hazard: dependency check, structural hazard: EX conflicts, control hazard: branch triggers fetch stall)
//    - IF: move to ID
//    - Fetch: bring new instructions if not stalled by a branch
// 3. Print total cycles, time in ms, and instruction mix
void Simulation::RunSimulation()
{
	int cycle = 0; // Track the current cycle number
	int finished_inst = 0; // Track how many instructions have finished processing
	int histogram[5] = {0, 0, 0, 0, 0}; // For counting the number of each instruction type (INT, FP, BRANCH, LOAD, STORE)A

	int EX_cycles_left[2] = {0, 0}; // For tracking how many cycles are left for instructions in EX stage (for D=2/3/4)

	Instruction *IF[2] = {nullptr, nullptr};
	Instruction *ID[2] = {nullptr, nullptr};
	Instruction *EX[2] = {nullptr, nullptr};
	Instruction *MEM[2] = {nullptr, nullptr};
	Instruction *WB[2] = {nullptr, nullptr};

	unordered_map<uint64_t, uint64_t> last_done_cycle; // For each PC, the cycle when the last instance became ready
	bool stall_fetch = false;						   // Whether to stall fetch in the current cycle
	bool stall_fetch_next = false;					   // Whether to stall fetch in the next cycle

	// While there are still instructions that haven't finished processing, continue the simulation cycle by cycle
	while (finished_inst < inst_count)
	{
		cycle++;

		// Check for instructions that have completed the WB stage in the current cycle
		// Update finished_inst and histogram counts, and free up WB stage
		for (int i = 0; i < 2; i++)
		{
			if (WB[i])
			{
				finished_inst++;
				histogram[WB[i]->type]++;
				WB[i] = nullptr;
			}
		}

		// Move instructions in MEM stage to WB stage if possible
		if (!WB[0] && MEM[0])
		{
			WB[0] = MEM[0];
			if (WB[0]->type == LOAD || WB[0]->type == STORE)
			{
				last_done_cycle[WB[0]->pc] = cycle;
			}
			MEM[0] = nullptr;
		}
		if (!WB[1] && MEM[1])
		{
			WB[1] = MEM[1];
			if (WB[1]->type == LOAD || WB[1]->type == STORE)
			{
				last_done_cycle[WB[1]->pc] = cycle;
			}
			MEM[1] = nullptr;
		}

		// Move instructions in EX stage to MEM stage if possible
		for (int i = 0; i < 2; i++){
			if (EX[i] && EX_cycles_left[i] > 0)
			{
				EX_cycles_left[i]--;
			}
		}
		bool mem_has_load = (MEM[0] && MEM[0]->type == LOAD) || (MEM[1] && MEM[1]->type == LOAD);
		bool mem_has_store = (MEM[0] && MEM[0]->type == STORE) || (MEM[1] && MEM[1]->type == STORE);
		if (EX[0])
		{
			bool has_space = !MEM[0] || !MEM[1];
			bool can_move = has_space;
			
			// Structural hazard check: MEM can only accept one load and one store per cycle
			if (EX[0]->type == LOAD && mem_has_load)
			{
				can_move = false;
			}
			else if (EX[0]->type == STORE && mem_has_store)
			{
				can_move = false;
			}

			// If instruction can move to MEM stage, update mem_has_load and mem_has_store
			if (can_move)
			{
				if(!MEM[0]){
					MEM[0] = EX[0];
				}else{
					MEM[1] = EX[0];
				}
				if(EX[0]->type == LOAD){
					mem_has_load = true;
				}else if(EX[0]->type == STORE){
					mem_has_store = true;
				}else{
					last_done_cycle[EX[0]->pc] = cycle;
				}
				EX[0] = nullptr;
			}
			if(EX[1]){
				has_space = !MEM[0] || !MEM[1];
				can_move = has_space;

				// Structural hazard check: MEM can only accept one load and one store per cycle
				if (EX[1]->type == LOAD && mem_has_load){
					can_move = false;
				}
				else if (EX[1]->type == STORE && mem_has_store){
					can_move = false;
				}
				if(can_move){
					if(!MEM[0]){
						MEM[0] = EX[1];
					}else{
						MEM[1] = EX[1];
					}
					if(EX[1]->type == LOAD){
						mem_has_load = true;
					}else if(EX[1]->type == STORE){
						mem_has_store = true;
					}else{
						last_done_cycle[EX[1]->pc] = cycle;
					}
					EX[1] = nullptr;
				}
			}
		}

		// If EX[0] is empty, allow EX[1] to move
		if(!EX[0] && EX[1]){
			bool has_space = !MEM[0] || !MEM[1];
			bool can_move = has_space;

			// Structural hazard check: MEM can only accept one load and one store per cycle
			if(EX[1]->type == LOAD && mem_has_load){
				can_move = false;
			}
			else if (EX[1]->type == STORE && mem_has_store){
				can_move = false;
			}
			if(can_move){
				if(!MEM[0]){
					MEM[0] = EX[1];
				}else{
					MEM[1] = EX[1];
				}
				if(EX[1]->type == LOAD){
					mem_has_load = true;
				}else if(EX[1]->type == STORE){
					mem_has_store = true;
				}else{
					last_done_cycle[EX[1]->pc] = cycle;
				}
				EX[1] = nullptr;
			}
		}

		// Move instructions in ID stage to EX stage if possible
		bool ex_has_int = (EX[0] && EX[0]->type == INT) || (EX[1] && EX[1]->type == INT);
		bool ex_has_fp = (EX[0] && EX[0]->type == FP) || (EX[1] && EX[1]->type == FP);
		bool ex_has_branch = (EX[0] && EX[0]->type == BRANCH) || (EX[1] && EX[1]->type == BRANCH);

		if(ID[0]){
			bool dependencies_satisfied = true;

			// Data hazard check: wait for producers to complete before issuing to EX
			for(uint64_t dependency_pc : ID[0]->dependences){
				unordered_map<uint64_t, uint64_t>::iterator it = last_done_cycle.find(dependency_pc);
				if(it != last_done_cycle.end()){
					if (it->second > cycle)
					{
						dependencies_satisfied = false;
						break;
					}
				}
			}

			// Structural hazard check: only one of each EX unit type can be active
			bool no_structural_hazard = true;
			if(ID[0]->type == INT && ex_has_int){
				no_structural_hazard = false;
			}else if(ID[0]->type == FP && ex_has_fp){
				no_structural_hazard = false;
			}else if(ID[0]->type == BRANCH && ex_has_branch){
				no_structural_hazard = false;
			}

			// Check if there is space in EX stage and move instruction from ID to EX if possible
			bool has_space = !EX[0] || !EX[1];
			if(dependencies_satisfied && no_structural_hazard && has_space){
				if(!EX[0]){
					EX[0] = ID[0];
				}else{
					EX[1] = ID[0];
				}
				
				// For D=2/3/4, set the number of cycles needed in EX stage if the instruction is FP
				int ex_cycles = (ID[0]->type == FP && (D_depth == 2 || D_depth == 4) ? 2 : 1); // For D=2/4, FP instructions take 2 cycles in EX stage
				if(!EX[0]){
					EX_cycles_left[0] = ex_cycles;
				}else{
					EX_cycles_left[1] = ex_cycles;
				}


				if(ID[0]->type == INT){
					ex_has_int = true;
				}else if(ID[0]->type == FP){
					ex_has_fp = true;
				}else if(ID[0]->type == BRANCH){
					ex_has_branch = true;
					// Control hazard: stall fetch in the next cycle after issuing a branch
					stall_fetch_next = true;
				}

				ID[0] = nullptr;

				// If ID[0] moved, try to process ID[1] in the same cycle
				if(ID[1]){
					dependencies_satisfied = true;

					// Data hazard check: wait for producers to complete before issuing to EX
					for(uint64_t dependency_pc : ID[1]->dependences){
						unordered_map<uint64_t, uint64_t>::iterator it = last_done_cycle.find(dependency_pc);
						if(it != last_done_cycle.end()){
							if (it->second > cycle)
							{
								dependencies_satisfied = false;
								break;
							}
						}
					}

					// Structural hazard check: only one of each EX unit type can be active
					no_structural_hazard = true;
					if(ID[1]->type == INT && ex_has_int){
						no_structural_hazard = false;
					}else if(ID[1]->type == FP && ex_has_fp){
						no_structural_hazard = false;
					}else if(ID[1]->type == BRANCH && ex_has_branch){
						no_structural_hazard = false;
					}

					// Check if there is space in EX stage and move instruction from ID to EX if possible
					has_space = !EX[0] || !EX[1];
					if(dependencies_satisfied && no_structural_hazard && has_space){
						if(!EX[0]){
							EX[0] = ID[1];
							EX_cycles_left[0] = ex_cycles;
						}else{
							EX[1] = ID[1];
							EX_cycles_left[1] = ex_cycles;
						}

						// Control hazard: stall fetch in the next cycle after issuing a branch
						if(ID[1]->type == BRANCH){
							stall_fetch_next = true;
						}

						ID[1] = nullptr;
					}
				}
			}
		}

		// Move instructions in IF stage to ID stage if possible
		if(!ID[0] && IF[0]){
			ID[0] = IF[0];
			IF[0] = nullptr;
			if(!ID[1] && IF[1]){
				ID[1] = IF[1];
				IF[1] = nullptr;
			}
		}

		// Fetch new instructions into IF stage if not stalled
		if (!stall_fetch){
			if(!IF[0] && InstructionQ->HasNext()){
				IF[0] = InstructionQ->GetNext();

			}
			if(!IF[1] && InstructionQ->HasNext()){
				IF[1] = InstructionQ->GetNext();
			}
		}

		// Update stall_fetch for next cycle
		stall_fetch = stall_fetch_next;
		stall_fetch_next = false;
	}

	// == Print total cycles and execution time in ms ==

	// Pick the clock speed based on D
	const double clock_ghz = (D_depth == 1) ? 1.0 : (D_depth == 2) ? 1.2 : (D_depth == 3) ? 1.7 : 1.8;

	// Convert cycles to time (seconds per cycle)
	const double seconds_per_cycle = 1.0 / (clock_ghz * 1e9);

	// Total time in milliseconds
	const double total_time_ms = static_cast<double>(cycle) * seconds_per_cycle * 1000.0;

	printf("Simulation finished in %d cycles\n", cycle);
	printf("Execution time: %.6f ms\n", total_time_ms);

	// Print instruction mix
	if (finished_inst > 0)
	{
		printf("Histogram (%%): int=%.2f, fp=%.2f, branch=%.2f, load=%.2f, store=%.2f\n",
			   100.0 * histogram[INT] / finished_inst,
			   100.0 * histogram[FP] / finished_inst,
			   100.0 * histogram[BRANCH] / finished_inst,
			   100.0 * histogram[LOAD] / finished_inst,
			   100.0 * histogram[STORE] / finished_inst);
	}
}

// Program's main function
int main(int argc, char* argv[]){

	// input arguments filename, start_inst, inst_count, D
	if(argc >= 5){

		string filename = argv[1];
		int start_inst = atoi(argv[2]);
        int inst_count = atoi(argv[3]);
        int D_depth = atoi(argv[4]);
   
	   	// Add error checks for input variables here, exit with exit code 1 if input is incorrect
        // Ensure filename exists, start_inst and inst_count are valid positive integers, and D_depth is between 1-4
		if (!filesystem::exists(filename) || start_inst <= 0 || inst_count <= 0 || D_depth < 1 || D_depth > 4){
            printf("Input Error. Terminating Simulation...\n");
			return 1;
        }
		
   		// If no input errors, initialize Simple Processor Pipeline Simulator
        printf("Running Simple Processor Pipeline Simulator with trace file '%s', start_inst = %d, inst_count = %d, D_depth = %d\n\n", 
            filename.c_str(), start_inst, inst_count, D_depth); 
   		Simulation* s = new Simulation(filename, start_inst, inst_count, D_depth);

   		// Start Simulation
		s->RunSimulation();

		delete s;
		return 0;
	}
	else {
		printf("Insufficient number of arguments provided!\n");
		return 1;
	}
}