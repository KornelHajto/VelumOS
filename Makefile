CC = g++
CFLAGS = -Wall -Iinclude

TARGET = node

SRCS = src/main.cpp src/network.cpp src/node_logic.cpp

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) src/*.o
