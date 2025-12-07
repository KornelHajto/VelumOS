#!/bin/bash

LOG_DIR="test_logs"
NUM_NODES=6

# Colors
BOLD='\033[1m'
NC='\033[0m'

echo -e "${BOLD}=== VelumOS Task Distribution Analysis ===${NC}"
printf "%-10s | %-15s | %-15s | %-15s\n" "NODE" "RECEIVED" "EXECUTED" "STATUS"
echo "----------------------------------------------------------------"

for ((i = 1; i <= NUM_NODES; i++)); do
  LOG="$LOG_DIR/node_$i.log"

  # Check if log exists
  if [ ! -f "$LOG" ]; then
    printf "%-10s | %-15s | %-15s | %-15s\n" "Node $i" "ERR" "ERR" "No Log"
    continue
  fi

  # 1. Count Tasks Received (Input from user)
  # We use 'cat' piped to grep to ensure we don't get filenames in output
  RCV=$(grep -c "Received command" "$LOG")

  # 2. Count Tasks Executed (Actually ran logic)
  # Looks for "Executing task locally" OR "Received offloaded task"
  # Adjust "Executing" to match your specific C++ log output
  EXEC=$(grep -c "Executing" "$LOG")

  # 3. Determine Role
  ROLE="Idle"
  if [ "$EXEC" -gt "$RCV" ]; then
    ROLE="Worker (Received Work)"
  elif [ "$RCV" -gt "$EXEC" ]; then
    ROLE="Manager (Offloaded Work)"
  elif [ "$EXEC" -gt 0 ]; then
    ROLE="Self-Managed"
  fi

  printf "%-10s | %-15d | %-15d | %-15s\n" "Node $i" "$RCV" "$EXEC" "$ROLE"
done
echo "----------------------------------------------------------------"
