CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g -I./src
LDFLAGS = -lm

# Source files
SRCS = src/main.c src/stl_parser.c src/slicer.c src/path_generator.c src/bvh.c src/convex_decomposition.c src/topology_evaluator.c
OBJS = $(SRCS:.c=.o)

# Test programs
TEST_SRCS = PSC_tests.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_TARGET = PSC_tests

# Target executable
TARGET = parametric_slicer

# Default target
all: run_tests

# Build everything
build: $(TARGET) $(TEST_TARGET)

# Run tests automatically after build
run_tests: build
	@echo "\nRunning tests..."
	./$(TEST_TARGET) A.stl

# Build the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Build test program
$(TEST_TARGET): $(TEST_OBJS) src/stl_parser.o src/topology_evaluator.o src/bvh.o src/convex_decomposition.o
	$(CC) $^ -o $@ $(LDFLAGS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST_TARGET) PSC_tests_log.txt

# Install dependencies (for development)
install-deps:
	# On Ubuntu/Debian: sudo apt-get install build-essential
	# On macOS: xcode-select --install
	# On Windows: Install MinGW or Visual Studio Build Tools
	@echo "Please install build tools for your system"

# Run the program
run: $(TARGET)
	./$(TARGET)

# Run tests
test: $(TEST_TARGET)
	./$(TEST_TARGET) A.stl

.PHONY: all clean install-deps run test build run_tests 