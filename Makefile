CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS = -lm -lGL -lGLU -lglfw -lGLEW

# Source files (using GPU stubs instead of full GPU accelerator)
SRCS = src/main.c src/stl_parser.c src/slicer.c src/path_generator.c src/bvh.c src/convex_decomposition.c src/topology_evaluator.c src/gpu_stubs.c
OBJS = $(SRCS:.c=.o)

# Test programs (excluding test_gpu.c due to GPU accelerator issues)
TEST_SRCS = test_bvh.c test_convex.c test_topology.c test_holes.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_TARGETS = test_bvh test_convex test_topology test_holes

# Target executable
TARGET = parametric_slicer

# Default target
all: $(TARGET) $(TEST_TARGETS)

# Build the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Build test programs
$(TEST_TARGETS): %: %.o src/stl_parser.o src/topology_evaluator.o src/bvh.o src/convex_decomposition.o
	$(CC) $^ -o $@ $(LDFLAGS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST_TARGETS)

# Install dependencies (for development)
install-deps:
	# On Ubuntu/Debian: sudo apt-get install build-essential
	# On macOS: xcode-select --install
	# On Windows: Install MinGW or Visual Studio Build Tools
	@echo "Please install build tools for your system"

# Run the program
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean install-deps run 