#!/bin/bash

# ==========================================
# VelumOS Heavy Load Test
# ==========================================

NUM_NODES=6
PIPE_DIR="test_pipes"
LOG_DIR="test_logs"

# Cleanup previous runs
pkill -f "./node"
rm -rf "$PIPE_DIR" "$LOG_DIR"
mkdir -p "$PIPE_DIR" "$LOG_DIR"

echo ">>> Launching Cluster..."
for (( i=1; i<=NUM_NODES; i++ ))
do
    mkfifo "$PIPE_DIR/node_$i.in"
    # Use stdbuf to ensure logs write instantly
    (tail -f "$PIPE_DIR/node_$i.in" | stdbuf -oL -eL ./node "$i" > "$LOG_DIR/node_$i.log" 2>&1) &
done

echo ">>> Waiting 3s for Mesh Sync..."
sleep 3

echo ">>> INJECTING HEAVY LOAD (MapReduce Style)..."

# We will spam Node 1 with 15 heavy tasks.
# It should distribute them to Nodes 2-6 because they are idle.
for j in {1..15}
do
    # Alternate between Pi and Primes
    if (( j % 2 == 0 )); then
        CMD="pi 300000"
    else
        CMD="prime 60000"
    fi
    
    echo "$CMD" > "$PIPE_DIR/node_1.in"
    echo " [Send] $CMD to Node 1"
    
    # Very short sleep: fast enough to flood Node 1, 
    # slow enough to not crash the pipe.
    sleep 0.2 
done

echo ">>> Waiting for tasks to finish..."
sleep 5

echo ">>> ANALYSIS:"
# Simple grep to see who did the work
for (( i=1; i<=NUM_NODES; i++ ))
do
    # Count how many times "Executing" appears in the logs
    COUNT=$(grep -c "Executing" "$LOG_DIR/node_$i.log")
    echo "Node $i Executed $COUNT tasks."
done

echo ">>> Done. Press Enter to clean up."
read
pkill -f "./node"
