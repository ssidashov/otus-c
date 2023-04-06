TARGET=word_count
BIN     := ./bin
EXE     := $(BIN)/$(TARGET)
SRC     := ./
OBJ     := ./obj
SRCS    := $(wildcard $(SRC)/*.c)
OBJS    := $(patsubst $(SRC)/%.c,$(OBJ)/%.o,$(SRCS))
NORMALIZER_TYPE_GCC_OPT =
ifeq ($(WORD_NORMALIZER), LIBC)
   NORMALIZER_TYPE_GCC_OPT = -DENABLE_UNICODE_USING_LOCALE
endif

.PHONY: all clean

all: $(EXE)

$(EXE): $(OBJS) | $(BIN)
	$(CC) $(LDFLAGS) $^ -o $@

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	$(CC) $(NORMALIZER_TYPE_GCC_OPT) $(CFLAGS) -Wall -Wextra -Wpedantic -std=c11 -c $< -o $@

$(BIN) $(OBJ):
	mkdir $@

clean:
	rm -Rf $(OBJ) $(BIN)
