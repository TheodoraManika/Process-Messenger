CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lrt

SRC_DIR = src
INC_DIR = include
BIN_DIR = bin
OBJ_DIR = obj

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/shm_init.c \
       $(SRC_DIR)/dialogue.c \
       $(SRC_DIR)/message.c \
       $(SRC_DIR)/sync.c

OBJS = $(OBJ_DIR)/main.o \
       $(OBJ_DIR)/shm_init.o \
       $(OBJ_DIR)/dialogue.o \
       $(OBJ_DIR)/message.o \
       $(OBJ_DIR)/sync.o

TARGET = $(BIN_DIR)/process_messenger

# Default target
all: $(TARGET)

# Create directories
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

# Compile object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

# Link executable
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Build successful: $(TARGET)"

# Run the program
run: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Cleaned build artifacts"

# Phony targets
.PHONY: all run clean