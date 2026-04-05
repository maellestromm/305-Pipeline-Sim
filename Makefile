# Makefile implemented with help from https://www.geeksforgeeks.org/cpp/makefile-in-c-and-its-applications/

# Define what compiler to use and the flags.
CC=cc
CXX=g++
CCFLAGS= -g -std=c++23 -Wall -Werror

# Target executable
TARGET = proj

# Source files
SRCS = proj.cpp InstructionQueue.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Default rule to buld and run the executable
all: $(TARGET) run

# decompress trace files
extract:
	gunzip compute_int_0.gz
	gunzip compute_fp_1.gz
	gunzip srv_0.gz

# link object files into the target executable
$(TARGET): $(OBJS)
	$(CXX) $(CCFLAGS) -o $(TARGET) $(OBJS)

# Compile all .cpp files into .o files
# % matches all (like * in a command)
# $< is the source file (.cpp file)
%.o : %.cpp
	$(CXX) -c $(CCFLAGS) $< -o $@

# Run the main executable
run: $(TARGET)
	$(TARGET)

clean:
	rm -f *.o proj