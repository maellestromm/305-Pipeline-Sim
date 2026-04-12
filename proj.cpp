#include "proj.h"

// Helper functions to keep pipeline stages left-aligned
void Simulation::CompactStage(Instruction *(&stage)[2])
{
	if (!stage[0] && stage[1])
	{
		stage[0] = stage[1];
		stage[1] = nullptr;
	}
}

// Helper function to keep pipeline stages left-aligned while also moving corresponding cycles_left values
void Simulation::CompactStageWithCycles(Instruction *(&stage)[2], int (&cycles)[2])
{
	if (!stage[0] && stage[1])
	{
		stage[0] = stage[1];
		cycles[0] = cycles[1];
		stage[1] = nullptr;
		cycles[1] = 0;
	}
}

// Global variable to hold the actual number of instructions loaded
int actual_inst_count = 0;

// Move instructions from IF to ID stage if possible, and fetch new instructions into IF stage if not stalled
void Simulation::ProcessIF(Instruction *(&IF)[2], Instruction *(&ID)[2], InstructionQueue *InstructionQ, bool &stall_fetch, bool &stall_fetch_next)
{
	// If ID[0] is empty, move IF[0] to ID[0]
	if (!ID[0] && IF[0])
	{
		ID[0] = IF[0];
		IF[0] = nullptr;

		// If ID[1] is empty, move IF[1] to ID[1]
		if (!ID[1] && IF[1])
		{
			ID[1] = IF[1];
			IF[1] = nullptr;
		}
	}

	// Fetch new instructions into IF stage if not stalled
	if (!stall_fetch)
	{
		if (!IF[0] && InstructionQ->HasNext())
		{
			IF[0] = InstructionQ->GetNext();
		}
		if (!IF[1] && InstructionQ->HasNext())
		{
			IF[1] = InstructionQ->GetNext();
		}
	}

	// Update stall_fetch for next cycle
	stall_fetch = stall_fetch_next;
	stall_fetch_next = false;
}

// Move instructions from ID to EX stage if possible, and update stall_fetch_next for the next cycle based on dependences
void Simulation::ProcessID(Instruction *(&ID)[2], Instruction *(&EX)[2], int (&EX_cycles_left)[2], int cycle, int D_depth, unordered_map<uint64_t, uint64_t> &last_done_cycle, bool &stall_fetch_next)
{
	bool ex_has_int = (EX[0] && EX[0]->type == INT) || (EX[1] && EX[1]->type == INT);		   // Whether there is an integer instruction in EX stage currently
	bool ex_has_fp = (EX[0] && EX[0]->type == FP) || (EX[1] && EX[1]->type == FP);			   // Whether there is a floating point instruction in EX stage currently
	bool ex_has_branch = (EX[0] && EX[0]->type == BRANCH) || (EX[1] && EX[1]->type == BRANCH); // Whether there is a branch instruction in EX stage currently

	if (ID[0])
	{
		bool dependencies_satisfied = true; // Whether all data dependencies have been satisfied

		// Data hazard check: wait for producers to complete before issuing to EX
		for (uint64_t dependency_pc : ID[0]->dependences)
		{
			// Look up the last cycle that an instruction with this dependency_pc completed in last_done_cycle
			unordered_map<uint64_t, uint64_t>::iterator it = last_done_cycle.find(dependency_pc);
			// If the producer hasn't completed by this cycle, the dependency isn't ready yet
			if (it != last_done_cycle.end())
			{
				if (it->second > static_cast<uint64_t>(cycle))
				{
					dependencies_satisfied = false;
					break;
				}
			}
		}

		// Structural hazard check: only one of each EX unit type can be active
		bool no_structural_hazard = true;
		if (ID[0]->type == INT && ex_has_int)
		{
			no_structural_hazard = false;
		}
		else if (ID[0]->type == FP && ex_has_fp)
		{
			no_structural_hazard = false;
		}
		else if (ID[0]->type == BRANCH && ex_has_branch)
		{
			no_structural_hazard = false;
		}

		bool has_space = !EX[0] || !EX[1]; // Whether there is space in EX stage
		// Move instruction from ID to EX if all dependencies are satisfied, there are no structural hazards, and there is space in EX stage
		if (dependencies_satisfied && no_structural_hazard && has_space)
		{
			int ex_slot = !EX[0] ? 0 : 1; // Choose the EX slot to move into (0 if available, otherwise 1)
			if (ex_slot == 0)
			{
				EX[0] = ID[0];
			}
			else
			{
				EX[1] = ID[0];
			}

			// For D=2/3/4, set the number of cycles needed in EX stage if the instruction is FP
			int ex_cycles = (ID[0]->type == FP && (D_depth == 2 || D_depth == 4) ? 2 : 1); // For D=2/4, FP instructions take 2 cycles in EX stage
			if (ex_slot == 0)
			{
				EX_cycles_left[0] = ex_cycles;
			}
			else
			{
				EX_cycles_left[1] = ex_cycles;
			}

			// Update ex_has_int, ex_has_fp, and ex_has_branch based on the instruction type
			if (ID[0]->type == INT)
			{
				ex_has_int = true;
			}
			else if (ID[0]->type == FP)
			{
				ex_has_fp = true;
			}
			else if (ID[0]->type == BRANCH)
			{
				ex_has_branch = true;
				// Control hazard: stall fetch in the next cycle after issuing a branch
				stall_fetch_next = true;
			}

			ID[0] = nullptr;

			// If ID[0] moved, try to process ID[1] in the same cycle
			if (ID[1])
			{
				dependencies_satisfied = true;

				// Data hazard check: wait for producers to complete before issuing to EX
				for (uint64_t dependency_pc : ID[1]->dependences)
				{
					// Look up the last cycle that an instruction with this dependency_pc completed in last_done_cycle
					unordered_map<uint64_t, uint64_t>::iterator it = last_done_cycle.find(dependency_pc);
					// If the producer hasn't completed by this cycle, the dependency isn't ready yet
					if (it != last_done_cycle.end())
					{
						if (it->second > static_cast<uint64_t>(cycle))
						{
							dependencies_satisfied = false;
							break;
						}
					}
				}

				// Structural hazard check: only one of each EX unit type can be active
				no_structural_hazard = true;
				if (ID[1]->type == INT && ex_has_int)
				{
					no_structural_hazard = false;
				}
				else if (ID[1]->type == FP && ex_has_fp)
				{
					no_structural_hazard = false;
				}
				else if (ID[1]->type == BRANCH && ex_has_branch)
				{
					no_structural_hazard = false;
				}

				// Check if there is space in EX stage and move instruction from ID to EX if possible
				has_space = !EX[0] || !EX[1];
				if (dependencies_satisfied && no_structural_hazard && has_space)
				{
					ex_cycles = (ID[1]->type == FP && (D_depth == 2 || D_depth == 4) ? 2 : 1); // For D=2/4, FP instructions take 2 cycles in EX stage
					ex_slot = !EX[0] ? 0 : 1;
					if (ex_slot == 0)
					{
						EX[0] = ID[1];
						EX_cycles_left[0] = ex_cycles;
					}
					else
					{
						EX[1] = ID[1];
						EX_cycles_left[1] = ex_cycles;
					}

					// Control hazard: stall fetch in the next cycle after issuing a branch
					if (ID[1]->type == BRANCH)
					{
						stall_fetch_next = true;
					}

					ID[1] = nullptr;
				}
			}
		}
	}
}

// Move instructions from EX to MEM stage if possible, and update EX_cycles_left and MEM_cycles_left for the next cycle based on D and instruction type
void Simulation::ProcessEX(Instruction *(&EX)[2], Instruction *(&MEM)[2], int (&EX_cycles_left)[2], int (&MEM_cycles_left)[2], int cycle, int D_depth, unordered_map<uint64_t, uint64_t> &last_done_cycle)
{
	for (int i = 0; i < 2; i++)
	{

		// Decrement EX and MEM cycles left for instructions currently in those stages
		if (EX[i] && EX_cycles_left[i] > 0)
		{
			EX_cycles_left[i]--;
		}
		if (MEM[i] && MEM_cycles_left[i] > 0)
		{
			MEM_cycles_left[i]--;
		}
	}

	bool mem_has_load = (MEM[0] && MEM[0]->type == LOAD) || (MEM[1] && MEM[1]->type == LOAD);	 // Whether there is a load instruction in MEM stage currently
	bool mem_has_store = (MEM[0] && MEM[0]->type == STORE) || (MEM[1] && MEM[1]->type == STORE); // Whether there is a store instruction in MEM stage currently

	if (EX[0])
	{
		bool has_space = !MEM[0] || !MEM[1]; // Whether there is space in MEM stage
		bool can_move = has_space;			 // Whether the instruction in EX[0] can move to MEM stage

		// Stall until EX finishes
		if (EX_cycles_left[0] > 0)
		{
			can_move = false;
		}

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
			if (!MEM[0])
			{
				MEM[0] = EX[0];
				int mem_cycles = (EX[0]->type == LOAD && (D_depth == 3 || D_depth == 4)) ? 3 : 1; // For D=3/4, LOAD instructions take 3 cycles in MEM stage
				MEM_cycles_left[0] = mem_cycles;
			}
			else
			{
				MEM[1] = EX[0];
				int mem_cycles = (EX[0]->type == LOAD && (D_depth == 3 || D_depth == 4)) ? 3 : 1; // For D=3/4, LOAD instructions take 3 cycles in MEM stage
				MEM_cycles_left[1] = mem_cycles;
			}

			if (EX[0]->type == LOAD)
			{
				mem_has_load = true;
			}
			else if (EX[0]->type == STORE)
			{
				mem_has_store = true;
			}
			else
			{
				last_done_cycle[EX[0]->pc] = cycle;
			}

			EX[0] = nullptr;
		}

		if (EX[1])
		{
			has_space = !MEM[0] || !MEM[1];
			can_move = has_space;

			// Stall until EX finishes
			if (EX_cycles_left[1] > 0)
			{
				can_move = false;
			}

			// Structural hazard check: MEM can only accept one load and one store per cycle
			if (EX[1]->type == LOAD && mem_has_load)
			{
				can_move = false;
			}
			else if (EX[1]->type == STORE && mem_has_store)
			{
				can_move = false;
			}

			// If instruction can move to MEM stage, update mem_has_load and mem_has_store
			if (can_move)
			{
				if (!MEM[0])
				{
					MEM[0] = EX[1];
					int mem_cycles = (EX[1]->type == LOAD && (D_depth == 3 || D_depth == 4)) ? 3 : 1; // For D=3/4, LOAD instructions take 3 cycles in MEM stage
					MEM_cycles_left[0] = mem_cycles;
				}
				else
				{
					MEM[1] = EX[1];
					int mem_cycles = (EX[1]->type == LOAD && (D_depth == 3 || D_depth == 4)) ? 3 : 1; // For D=3/4, LOAD instructions take 3 cycles in MEM stage
					MEM_cycles_left[1] = mem_cycles;
				}

				if (EX[1]->type == LOAD)
				{
					mem_has_load = true;
				}
				else if (EX[1]->type == STORE)
				{
					mem_has_store = true;
				}
				else
				{
					last_done_cycle[EX[1]->pc] = cycle;
				}

				EX[1] = nullptr;
			}
		}
	}

	// If EX[0] is empty, allow EX[1] to move
	if (!EX[0] && EX[1])
	{
		bool has_space = !MEM[0] || !MEM[1]; // Whether there is space in MEM stage
		bool can_move = has_space;			 // Whether the instruction in EX[1] can move to MEM stage

		// Stall until EX finishes
		if (EX_cycles_left[1] > 0)
		{
			can_move = false;
		}

		// Structural hazard check: MEM can only accept one load and one store per cycle
		if (EX[1]->type == LOAD && mem_has_load)
		{
			can_move = false;
		}
		else if (EX[1]->type == STORE && mem_has_store)
		{
			can_move = false;
		}

		// If instruction can move to MEM stage, update mem_has_load and mem_has_store
		if (can_move)
		{
			if (!MEM[0])
			{
				MEM[0] = EX[1];
				int mem_cycles = (EX[1]->type == LOAD && (D_depth == 3 || D_depth == 4)) ? 3 : 1; // For D=3/4, LOAD instructions take 3 cycles in MEM stage
				MEM_cycles_left[0] = mem_cycles;
			}
			else
			{
				MEM[1] = EX[1];
				int mem_cycles = (EX[1]->type == LOAD && (D_depth == 3 || D_depth == 4)) ? 3 : 1; // For D=3/4, LOAD instructions take 3 cycles in MEM stage
				MEM_cycles_left[1] = mem_cycles;
			}

			if (EX[1]->type == LOAD)
			{
				mem_has_load = true;
			}
			else if (EX[1]->type == STORE)
			{
				mem_has_store = true;
			}
			else
			{
				last_done_cycle[EX[1]->pc] = cycle;
			}

			EX[1] = nullptr;
		}
	}
}

// Move instructions from MEM to WB stage if possible
void Simulation::ProcessMEM(Instruction *(&MEM)[2], Instruction *(&WB)[2], int (&MEM_cycles_left)[2], unordered_map<uint64_t, uint64_t> &last_done_cycle, int cycle)
{
	// Check if MEM[0] can move to WB stage (only if WB[0] is empty, MEM[0] has an instruction, and MEM[0] has finished any required MEM cycles)
	if (!WB[0] && MEM[0] && MEM_cycles_left[0] == 0)
	{
		WB[0] = MEM[0];

		// If instruction is a load/store, update last_done_cycle for that PC to the current cycle
		if (WB[0]->type == LOAD || WB[0]->type == STORE)
		{
			last_done_cycle[WB[0]->pc] = cycle;
		}
		MEM[0] = nullptr;
	}
	// Check if MEM[1] can move to WB stage (only if WB[1] is empty, MEM[1] has an instruction, MEM[1] has finished any required MEM cycles, and MEM[0] is not moving to WB in the same cycle)
	if (!WB[1] && MEM[1] && MEM_cycles_left[1] == 0 && !MEM[0])
	{
		WB[1] = MEM[1];

		// If instruction is a load/store, update last_done_cycle for that PC to the current cycle
		if (WB[1]->type == LOAD || WB[1]->type == STORE)
		{
			last_done_cycle[WB[1]->pc] = cycle;
		}
		MEM[1] = nullptr;
	}
}

// Move instructions from WB to Done stage, and update retired_inst and histogram counts for the next cycle
void Simulation::ProcessWB(Instruction *(&WB)[2], int &retired_inst, int (&histogram)[5])
{
	for (int i = 0; i < 2; i++)
	{
		// Update finished_inst and histogram counts, and free up WB stage
		if (WB[i])
		{
			retired_inst++;
			histogram[WB[i]->type]++;
			WB[i] = nullptr;
		}
	}
}

// Overall flow:
// 1. Read the trace into the instruction queue
// 2. Run cycle by cycle until all instructions finish
//    - WB: retire + histogram update
//    - MEM: move to WB
//    - EX: move to MEM (structural hazard: load/store MEM conflicts)
//    - ID: move to EX (data hazard: dependency check, structural hazard: EX conflicts, control hazard: branch triggers fetch stall)
//    - IF: move to ID + fetch (structural hazard: ID conflicts, control hazard: stalled fetch)
// 3. Print total cycles, time in ms, and instruction mix
void Simulation::RunSimulation()
{
	/* Initialize simulation variables */
	int cycle = 0;						// Track the current cycle number
	int retired_inst = 0;				// Track how many instructions have been retired
	int histogram[5] = {0, 0, 0, 0, 0}; // For counting the number of each instruction type (INT, FP, BRANCH, LOAD, STORE)

	int EX_cycles_left[2] = {0, 0};	 // For tracking how many cycles are left for instructions in EX stage (for D=2/3/4)
	int MEM_cycles_left[2] = {0, 0}; // For tracking how many cycles are left for instructions in MEM stage (for D=3/4)

	// For tracking instructions in each pipeline stage
	Instruction *IF[2] = {nullptr, nullptr};
	Instruction *ID[2] = {nullptr, nullptr};
	Instruction *EX[2] = {nullptr, nullptr};
	Instruction *MEM[2] = {nullptr, nullptr};
	Instruction *WB[2] = {nullptr, nullptr};

	unordered_map<uint64_t, uint64_t> last_done_cycle; // For tracking the last cycle when an instruction with a given PC completed
	bool stall_fetch = false;						   // Whether to stall fetch in the current cycle
	bool stall_fetch_next = false;					   // Whether to stall fetch in the next cycle

	// While there are still instructions that haven't finished processing, continue the simulation
	while (retired_inst < inst_count)
	{
		cycle++;

		// Compact pipeline stages to keep them left-aligned after movements each cycle
		CompactStage(IF);
		CompactStage(ID);
		CompactStageWithCycles(EX, EX_cycles_left);
		CompactStageWithCycles(MEM, MEM_cycles_left);
		CompactStage(WB);

		// WB -> Done: Move instructions in WB stage to Done stage and update retired_inst and histogram counts
		ProcessWB(WB, retired_inst, histogram);
		CompactStage(WB);

		// MEM -> WB: Move instructions in MEM stage to WB stage if possible
		ProcessMEM(MEM, WB, MEM_cycles_left, last_done_cycle, cycle);
		CompactStageWithCycles(MEM, MEM_cycles_left);
		CompactStage(WB);

		// EX -> MEM: Move instructions in EX stage to MEM stage if possible
		ProcessEX(EX, MEM, EX_cycles_left, MEM_cycles_left, cycle, D_depth, last_done_cycle);
		CompactStageWithCycles(EX, EX_cycles_left);
		CompactStageWithCycles(MEM, MEM_cycles_left);

		// ID -> EX: Move instructions in ID stage to EX stage if possible
		ProcessID(ID, EX, EX_cycles_left, cycle, D_depth, last_done_cycle, stall_fetch_next);
		CompactStage(ID);
		CompactStageWithCycles(EX, EX_cycles_left);

		// IF -> ID: Move instructions in IF stage to ID stage if possible
		ProcessIF(IF, ID, InstructionQ, stall_fetch, stall_fetch_next);
		CompactStage(IF);
		CompactStage(ID);
	}

	/* Print total cycles and execution time in ms */

	// Pick the clock speed based on D
	const double clock_ghz = (D_depth == 1) ? 1.0 : (D_depth == 2) ? 1.2
												: (D_depth == 3)   ? 1.7
																   : 1.8;

	// Convert cycles to time (seconds per cycle)
	const double seconds_per_cycle = 1.0 / (clock_ghz * 1e9);

	// Total time in milliseconds
	const double total_time_ms = static_cast<double>(cycle) * seconds_per_cycle * 1000.0;

	printf("Simulation finished in %d cycles\n", cycle);
	printf("Execution time: %.6f ms\n", total_time_ms);

	// Print instruction mix
	if (retired_inst > 0)
	{
		printf("Histogram (%%): int=%.2f, fp=%.2f, branch=%.2f, load=%.2f, store=%.2f\n",
			   100.0 * histogram[INT] / retired_inst,
			   100.0 * histogram[FP] / retired_inst,
			   100.0 * histogram[BRANCH] / retired_inst,
			   100.0 * histogram[LOAD] / retired_inst,
			   100.0 * histogram[STORE] / retired_inst);
	}
}

// Program's main function
int main(int argc, char *argv[])
{

	// input arguments filename, start_inst, inst_count, D
	if (argc >= 5)
	{

		string filename = argv[1];
		int start_inst = atoi(argv[2]);
		int inst_count = atoi(argv[3]);
		int D_depth = atoi(argv[4]);

		// Add error checks for input variables here, exit with exit code 1 if input is incorrect
		// Ensure filename exists, start_inst and inst_count are valid positive integers, and D_depth is between 1-4
		if (!filesystem::exists(filename) || start_inst <= 0 || inst_count <= 0 || D_depth < 1 || D_depth > 4)
		{
			printf("Input Error. Terminating Simulation...\n");
			return 1;
		}

		// If no input errors, initialize Simple Processor Pipeline Simulator
		printf("Running Simple Processor Pipeline Simulator with trace file '%s', start_inst = %d, inst_count = %d, D_depth = %d\n\n",
			   filename.c_str(), start_inst, inst_count, D_depth);
		Simulation *s = new Simulation(filename, start_inst, inst_count, D_depth);

		// Use the actual number of instructions loaded (may be less than inst_count)
		if (actual_inst_count < inst_count)
		{
			printf("Warning: Only %d instructions loaded (requested %d).\n", actual_inst_count, inst_count);
			inst_count = actual_inst_count;
		}

		// Start Simulation
		s->RunSimulation();

		delete s;
		return 0;
	}
	else
	{
		printf("Insufficient number of arguments provided!\n");
		return 1;
	}
}
