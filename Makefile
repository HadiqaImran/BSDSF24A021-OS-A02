CC=gcc
CFLAGS=-Wall -g
SRC=src/ls-v1.0.0.c
TARGET=bin/ls

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) obj/*.o
