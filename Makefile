CC = gcc
CFLAGS = -Wall -Wextra -Iinclude -Dsize=st_size -MMD -MP
SRC = src/main.c src/vcl.c src/vec.c src/registry.c src/scanner.c
OBJ = $(SRC:.c=.o)
DEP = $(OBJ:.o=.d)
TARGET = llfpl1

FILE ?= tests/baseline.lp

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET) $(FILE)

clean:
	rm -f $(OBJ) $(DEP) $(TARGET)

-include $(DEP)

.PHONY: all run clean
