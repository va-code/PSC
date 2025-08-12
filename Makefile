CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g -I./src
LDFLAGS = -lm

# Directories
BUILD_DIR = build
TEST_DIR = tests

# Source files
SRCS = src/main.c src/stl_parser.c src/slicer.c src/path_generator.c src/bvh.c src/convex_decomposition.c src/topology_evaluator.c
OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Test programs
TEST_SRCS = $(TEST_DIR)/PSC_tests.c
TEST_OBJS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/%.o,$(TEST_SRCS))
TEST_TARGET = $(BUILD_DIR)/PSC_tests

# Target executable
TARGET = $(BUILD_DIR)/parametric_slicer

# Default target
all: $(TARGET) $(TEST_TARGET)
	@echo "\nRunning tests..."
	$(TEST_TARGET) A.stl

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build the executable
$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Build test program
$(TEST_TARGET): $(TEST_OBJS) $(BUILD_DIR)/stl_parser.o $(BUILD_DIR)/topology_evaluator.o $(BUILD_DIR)/bvh.o $(BUILD_DIR)/convex_decomposition.o | $(BUILD_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

# Compile source files
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	rm -f PSC_tests_log.txt

# Install dependencies (for development)
install-deps:
	# On Ubuntu/Debian: sudo apt-get install build-essential
	# On macOS: xcode-select --install
	# On Windows: Install MinGW or Visual Studio Build Tools
	@echo "Please install build tools for your system"

# Run the program
run: $(TARGET)
	$(TARGET)

# Run tests
test: $(TEST_TARGET)
	$(TEST_TARGET) A.stl

.PHONY: all clean install-deps run test