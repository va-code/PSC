CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS = -lm -lGL -lGLU -lglfw -lGLEW

# Source files
SRCS = src/main.c src/stl_parser.c src/slicer.c src/path_generator.c src/bvh.c src/convex_decomposition.c src/topology_evaluator.c src/gpu_accelerator.c
OBJS = $(SRCS:.c=.o)

# Target executable
TARGET = parametric_slicer

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -f $(OBJS) $(TARGET)

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