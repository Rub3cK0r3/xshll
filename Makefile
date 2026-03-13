CFLAGS ?= -Wall -Wextra -Wpedantic -std=c11 -O2
CPPFLAGS ?= -Iinclude

BIN := xshll
SRC := src/main.c src/parser.c src/pipeline.c src/builtin.c src/history.c src/signals.c
OBJ := $(SRC:.c=.o)

.PHONY: all clean release

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BIN) $(OBJ)

release: CFLAGS += -O2 -DNDEBUG
release: clean all

