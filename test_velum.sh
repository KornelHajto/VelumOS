#!/bin/bash

# ==========================================
# VelumOS Distributed System Test Suite
# ==========================================

# Configuration
NUM_NODES=6           # Number of nodes to launch (5 to 10)
CMDS_PER_NODE=5       # Commands to send per node
SYNC_TIME=3           # Seconds to wait for mesh discovery
LOG_DIR="test_logs"   # Directory to store output
PIPE_DIR="test_pipes" # Directory for input pipes

# Colors for pretty output
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# 1. Cleanup Function
cleanup() {
  echo -e "\n${RED}[!] Stopping all nodes...${NC}"
  pkill -f "./node"
  rm -rf "$PIPE_DIR"
  echo -e "${GREEN}[✓] Cleanup complete.${NC}"
}

# Trap Ctrl+C to ensure cleanup works if user aborts
trap cleanup EXIT

# 2. Preparation
echo -e "${CYAN}=== VelumOS Automation Test ===${NC}"

# Compile first to ensure latest binary
echo -e "${YELLOW}[*] Compiling project...${NC}"
make clean >/dev/null 2>&1 && make >/dev/null 2>&1
if [ $? -ne 0 ]; then
  echo -e "${RED}[X] Compilation failed. Check your C++ code.${NC}"
  exit 1
fi
echo -e "${GREEN}[✓] Compilation successful.${NC}"

# Setup directories
rm -rf "$LOG_DIR" "$PIPE_DIR"
mkdir -p "$LOG_DIR"
mkdir -p "$PIPE_DIR"

# 3. Launch Nodes
echo -e "${YELLOW}[*] Launching $NUM_NODES nodes...${NC}"

for ((i = 1; i <= NUM_NODES; i++)); do
  # Create a named pipe for this node's input
  mkfifo "$PIPE_DIR/node_$i.in"

  # Start the node in the background:
  # 1. Read input from the pipe
  # 2. Write output to a log file
  # 3. Use a file descriptor loop to keep the pipe open
  (tail -f "$PIPE_DIR/node_$i.in" | ./node "$i" >"$LOG_DIR/node_$i.log" 2>&1) &

  echo "    -> Node $i started (PID: $!)"
done

# 4. Mesh Synchronization
echo -e "${YELLOW}[*] Waiting ${SYNC_TIME}s for gossip protocol to sync mesh...${NC}"
sleep $SYNC_TIME
echo -e "${GREEN}[✓] Mesh should be stable.${NC}"

# 5. Inject Commands
echo -e "${YELLOW}[*] Injecting $CMDS_PER_NODE commands per node...${NC}"

# Operations array
OPS=("add" "sub" "mul" "div")

for ((i = 1; i <= NUM_NODES; i++)); do
  for ((j = 1; j <= CMDS_PER_NODE; j++)); do
    # Generate random operation and numbers
    OP=${OPS[$RANDOM % ${#OPS[@]}]}
    A=$((1 + RANDOM % 100))
    B=$((1 + RANDOM % 20)) # Keep B small to avoid div by zero issues easily

    CMD="$OP $A $B"

    # Inject command into the specific node's pipe
    echo "$CMD" >"$PIPE_DIR/node_$i.in"

    # Small delay to prevent flooding stdout too fast (optional)
    sleep 0.1
  done
  echo "    -> Sent $CMDS_PER_NODE commands to Node $i"
done

# 6. Analysis
echo -e "${YELLOW}[*] Waiting for tasks to complete processing...${NC}"
sleep 2

echo -e "${CYAN}=== Test Summary ===${NC}"
echo "Logs are stored in ./$LOG_DIR/"
echo "Checking for successful executions..."

# Grep through logs to find results (Assumes your node prints "Result:" or similar)
# Since I don't know your exact output format, I will list line counts.
for ((i = 1; i <= NUM_NODES; i++)); do
  LINES=$(wc -l <"$LOG_DIR/node_$i.log")
  echo "Node $i Log: $LINES lines of activity."
done

echo -e "${GREEN}[✓] Test sequence finished.${NC}"
echo -e "Press [ENTER] to stop the nodes and clean up."
read

# Cleanup happens automatically due to trap
