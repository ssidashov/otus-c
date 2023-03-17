TARGET=iconv
CFLAGS=-Wall -Wextra -Wpedantic -std=c11
BIN     := ./bin
EXE     := $(BIN)/$(TARGET)
SRC     := ./
OBJ     := ./obj
SRCS    := $(wildcard $(SRC)/*.c)
OBJS    := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))

.PHONY: all clean

all: clean $(EXE)

$(EXE): $(OBJS) | $(BIN)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN) $(OBJ):
	mkdir $@

clean:
	rm -Rf $(OBJ) $(BIN)
