# Ορισμός του Compiler
CC = gcc

# Flags για τον Compiler (Warnings και Debugging info)
CFLAGS = -Wall -Wextra -g

# Flags για τον Linker (Βιβλιοθήκες για Real-time και Threads)
LDFLAGS = -lrt -lpthread

# Το όνομα του τελικού εκτελέσιμου (τοποθετημένο στον φάκελο bin)
BIN_DIR = bin
TARGET = $(BIN_DIR)/chat_app

# Διευθύνσεις πηγών και αντικειμένων
SRCS = src/main.c src/ipc.c
OBJ_DIR = obj

# Μετατροπή των .c σε .o μέσα στον κατάλογο $(OBJ_DIR)
OBJS = $(patsubst src/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Κεφαλίδες (αν αλλάξουν, πρέπει να ξαναγίνει compile)
HEADERS = include/defs.h include/ipc.h

# Ο βασικός κανόνας (default)
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

# Κανόνας για τη δημιουργία των object files (.o) μέσα στον $(OBJ_DIR)
# Το $< είναι το αρχείο εισόδου (.c) και το $@ είναι το αρχείο εξόδου (.o)
$(OBJ_DIR)/%.o: src/%.c $(HEADERS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Κανόνας για καθαρισμό (διαγραφή compiled αρχείων)
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Κανόνας για γρήγορο τρέξιμο (προαιρετικό)
run: $(TARGET)
	./$(TARGET)

# Κανόνας για έλεγχο μνήμης με Valgrind (προαιρετικό)
valgrind: $(TARGET)
	valgrind --leak-check=full --track-origins=yes ./$(TARGET)