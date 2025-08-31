CC = gcc
# Debug flags enabled by default: -g3 for maximum debug info, -O0 to disable optimization
# -fno-omit-frame-pointer helps with backtraces
CFLAGS = -Wall -Wextra -std=c99 -g3 -O0 -fno-omit-frame-pointer -I./src
LDFLAGS = -lm -lGL -lGLEW -lglfw

# Directories
BUILD_DIR = build
TEST_DIR = tests

# Source files for parametric_slicer (excluding convex decomposition)
SRCS = src/main.c src/stl_parser.c src/topology_evaluator.c src/PSC_model_inspector.c src/mesh_adjacency.c
OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Convex decomposition program
CONVEX_SRCS = src/convex_main.c src/stl_parser.c src/topology_evaluator.c src/PSC_model_inspector.c src/convex_decomposition.c src/mesh_adjacency.c
CONVEX_OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(CONVEX_SRCS))
CONVEX_TARGET = $(BUILD_DIR)/convex_decomposition

# Model inspector test program
TEST_SRCS = $(TEST_DIR)/PSC_tests.c
TEST_OBJS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/%.o,$(TEST_SRCS))
TEST_TARGET = $(BUILD_DIR)/PSC_model_inspector

# Target executable
TARGET = $(BUILD_DIR)/parametric_slicer

# Default target - runs tests with model inspector, then convex decomposition
all: $(TARGET) $(TEST_TARGET) $(CONVEX_TARGET)
	@echo "Running tests with model inspector..."
	$(TEST_TARGET) A.stl
	@echo "Running convex decomposition visualization..."
	$(CONVEX_TARGET) A.stl 0.8 3

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build the parametric_slicer executable (without convex decomposition)
$(TARGET): $(OBJS) $(BUILD_DIR)/convex_decomposition.o | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) $(BUILD_DIR)/convex_decomposition.o -o $@ $(LDFLAGS)

# Build convex decomposition program
$(CONVEX_TARGET): $(CONVEX_OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CONVEX_OBJS) -o $@ $(LDFLAGS)

# Build test program  
$(TEST_TARGET): $(TEST_OBJS) $(BUILD_DIR)/stl_parser.o $(BUILD_DIR)/topology_evaluator.o $(BUILD_DIR)/PSC_model_inspector.o $(BUILD_DIR)/convex_decomposition.o | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Compile source files
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	rm -f PSC_model_inspector_log.txt

# Install dependencies (for development)
install-deps:
	# On Ubuntu/Debian: sudo apt-get install build-essential
	# On macOS: xcode-select --install
	# On Windows: Install MinGW or Visual Studio Build Tools
	@echo "Please install build tools for your system"

# Run the program
run: $(TARGET)
	$(TARGET)

# Run tests with model inspector
test: $(TEST_TARGET)
	$(TEST_TARGET) A.stl

# Run tests only (no model inspector)
test-only: $(TEST_TARGET)
	$(TEST_TARGET) A.stl --no-inspector

.PHONY: all clean install-deps run test test-only