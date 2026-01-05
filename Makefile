# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -mwindows
LIBS = -lcomctl32

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build

# Target executable
TARGET = scanner.exe

# Source files
SOURCES = main.c $(SRC_DIR)/gui.c $(SRC_DIR)/scanner.c $(SRC_DIR)/freezer.c $(SRC_DIR)/process_manager.c

# Object files (derived from source files)
OBJECTS = $(BUILD_DIR)/main.o $(BUILD_DIR)/gui.o $(BUILD_DIR)/scanner.o $(BUILD_DIR)/freezer.o $(BUILD_DIR)/process_manager.o

# Header files for dependency tracking
HEADERS = $(INC_DIR)/types.h $(INC_DIR)/gui.h $(INC_DIR)/scanner.h $(INC_DIR)/freezer.h $(INC_DIR)/process_manager.h

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS) $(LIBS)

# Compile main.c (top level)
$(BUILD_DIR)/main.o: main.c $(HEADERS)
	$(CC) $(CFLAGS) -c main.c -o $(BUILD_DIR)/main.o

# Compile source files from src/ directory
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)
	rm -f *.o

# Rebuild everything
rebuild: clean all

# Run the program
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean rebuild run
