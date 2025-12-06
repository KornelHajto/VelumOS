CC = g++
CFLAGS = -Wall -Iinclude -Icommon -Os -s

TARGET = node

SRCS = src/main.cpp src/network.cpp common/node_logic.cpp

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) src/*.o
