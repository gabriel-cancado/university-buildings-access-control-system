CC = gcc
SRC_DIR = src
OBJ_DIR = bin
BIN_DIR = bin

all: $(BIN_DIR)/server $(BIN_DIR)/client

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -c $< -o $@

$(BIN_DIR)/server: $(OBJ_DIR)/server.o $(OBJ_DIR)/common.o
	$(CC) $^ -o $@

$(BIN_DIR)/client: $(OBJ_DIR)/client.o $(OBJ_DIR)/common.o
	$(CC) $^ -o $@

clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/*