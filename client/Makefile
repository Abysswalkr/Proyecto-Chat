CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS = -lcjson

SRC = client.c
OBJ = $(SRC:.c=.o)
TARGET = client

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
