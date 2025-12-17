# Ορισμός του Compiler
CC = gcc

# Flags για τον Compiler (Warnings και Debugging info)
CFLAGS = -Wall -Wextra -g

# Ανίχνευση Λειτουργικού
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    LDFLAGS := -lrt -lpthread -lncurses
else
    LDFLAGS := -lpthread -lncurses
endif

SRC_DIR := src
MOD_DIR := modules
OBJ_DIR := obj
BIN_DIR := bin
TARGET := $(BIN_DIR)/chat_app

# discover sources automatically
SRCS := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(MOD_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(filter $(SRC_DIR)/%.c,$(SRCS))) \
        $(patsubst $(MOD_DIR)/%.c,$(OBJ_DIR)/%.o,$(filter $(MOD_DIR)/%.c,$(SRCS)))


.PHONY: all clean run valgrind clear_ipc shm_dump_run

all: $(TARGET)

# Δημιουργία φακέλου αντικειμένων
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# Δημιουργία φακέλου για το εκτελέσιμο
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Κανόνας για τη δημιουργία του εκτελέσιμου (Linking)
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)


# compile rules for sources in src/ and modules/
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(MOD_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

# Κανόνας για έλεγχο μνήμης με Valgrind (προαιρετικό)
valgrind: $(TARGET)
	valgrind --leak-check=full --track-origins=yes ./$(TARGET)