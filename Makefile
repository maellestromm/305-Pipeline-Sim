# Define what compiler to use and the flags.
CC=cc
CXX=g++
CCFLAGS= -g -std=c++23 -Wall -Werror


all: proj

# decompress trace files
extract:
	gunzip compute_int_0.gz
	gunzip compute_fp_1.gz
	gunzip srv_0.gz

# Compile all .cpp files into .o files
# % matches all (like * in a command)
# $< is the source file (.cpp file)
%.o : %.cpp
	$(CXX) -c $(CCFLAGS) $<


proj: proj.o 
	$(CC) -o proj proj.o -lm -lstdc++


clean:
	rm -f *.o proj