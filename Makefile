CC = g++
CFLAGS = -Wall -Iinclude -Icommon -Os -s

TARGET = node

SRCS = src/main.cpp src/network.cpp src/node_logic.cpp src/user_tasks.cpp src/velum_core.cpp

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test_bin: tests/unit_tests.cpp src/node_logic.cpp src/user_tasks.cpp
	$(CXX) $(CXXFLAGS) -o run_tests tests/unit_tests.cpp src/node_logic.cpp src/user_tasks.cpp -lgtest -lgtest_main -pthread

test: test_bin
	./run_tests

clean:
	rm -f $(TARGET) src/*.o
