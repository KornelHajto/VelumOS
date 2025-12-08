# Compilers
CXX = g++
CC = gcc

# Flags
# -lm is for math library (required by Wasm3)
# -pthread is for your background threads
CXXFLAGS = -std=c++17 -Wall -Iinclude -Icommon -pthread -Os
CFLAGS = -std=c99 -Wall -Iinclude -Icommon -O3 -Dd_m3HasWASI

TARGET = node

# 1. Your C++ Files
CPP_SRCS = src/main.cpp \
           src/network.cpp \
           src/node_logic.cpp \
           src/user_tasks.cpp \
           src/velum_core.cpp

# 2. Wasm3 C Files (The Engine)
C_SRCS = src/m3_core.c \
         src/m3_env.c \
         src/m3_exec.c \
         src/m3_function.c \
         src/m3_info.c \
         src/m3_module.c \
         src/m3_parse.c \
         src/m3_bind.c \
         src/m3_code.c \
         src/m3_compile.c \
         src/m3_api_libc.c \
         src/m3_api_tracer.c

# Generate Object Names automatically
OBJS = $(CPP_SRCS:.cpp=.o) $(C_SRCS:.c=.o)

# Main Build Rule
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) -lm

# Compile C++ files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile C files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Unit Tests (Uses GoogleTest)
test_bin: tests/unit_tests.cpp src/node_logic.cpp src/user_tasks.cpp
	$(CXX) $(CXXFLAGS) -o run_tests tests/unit_tests.cpp src/node_logic.cpp src/user_tasks.cpp -lgtest -lgtest_main -pthread

test: test_bin
	./run_tests

clean:
	rm -f $(TARGET) run_tests src/*.o
