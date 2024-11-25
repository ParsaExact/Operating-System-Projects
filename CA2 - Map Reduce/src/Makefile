# Define the compiler
CXX = g++

# Define the compiler flags
CXXFLAGS = -Wall -g

# Define the target executable names
TARGETS = main warehouse product

# Define the source files
SRCS = main.cpp warehouse.cpp product.cpp log.cpp

# Define the object files
OBJS = $(SRCS:.cpp=.o)

# Default target to build all executables
all: $(TARGETS)

# Rule to build the main executable
main: main.o log.o
	$(CXX) $(CXXFLAGS) -o main main.o log.o

# Rule to build the warehouse executable
warehouse: warehouse.o log.o
	$(CXX) $(CXXFLAGS) -o warehouse warehouse.o log.o

# Rule to build the product executable
product: product.o log.o
	$(CXX) $(CXXFLAGS) -o product product.o log.o

# Rule to build object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to clean up the build directory
clean:
	rm -f $(OBJS) $(TARGETS)
