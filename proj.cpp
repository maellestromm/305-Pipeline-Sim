#include "proj.h"

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
        //s->RunSimulation();

		delete s;
		return 0;
	}
	else {
		printf("Insufficient number of arguments provided!\n");
		return 1;
	}
}