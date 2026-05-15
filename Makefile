CC = gcc
CFLAGS = -Wall -Wextra -Iinclude
SRC = src/main.c src/vcl.c src/vec.c src/registry.c
OBJ = $(SRC:.c=.o)
TARGET = llfpl1

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all run clean
