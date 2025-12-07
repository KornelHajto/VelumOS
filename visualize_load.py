import os
import re
import matplotlib.pyplot as plt

LOG_DIR = "test_logs"
NODES = 6

# Regex to catch "Executing" and "COMPLETE"
# We assume the log has timestamps or we estimate based on line position
# Since we don't have exact timestamps in the log prints, we will simply count tasks.

def parse_logs():
    node_counts = []
    node_labels = []

    print(f"Reading logs from {LOG_DIR}...")

    for i in range(1, NODES + 1):
        filename = f"{LOG_DIR}/node_{i}.log"
        count = 0
        if os.path.exists(filename):
            with open(filename, 'r') as f:
                content = f.read()
                # Count occurrences of "Executing"
                count = len(re.findall(r"Executing", content))
        
        node_counts.append(count)
        node_labels.append(f"Node {i}")

    return node_labels, node_counts

def plot_distribution(labels, counts):
    plt.figure(figsize=(10, 6))
    
    # Create Bar Chart
    bars = plt.bar(labels, counts, color=['#FF6B6B', '#4ECDC4', '#4ECDC4', '#4ECDC4', '#4ECDC4', '#4ECDC4'])
    
    plt.xlabel('Cluster Nodes', fontsize=12)
    plt.ylabel('Tasks Executed', fontsize=12)
    plt.title('VelumOS: Load Distribution (Heavy RPC Tasks)', fontsize=14, fontweight='bold')
    plt.grid(axis='y', linestyle='--', alpha=0.7)
    
    # Add numbers on top of bars
    for bar in bars:
        yval = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2, yval + 0.1, int(yval), ha='center', va='bottom', fontweight='bold')

    print("Saving graph to 'load_distribution.png'...")
    plt.savefig('load_distribution.png')
    plt.show()

if __name__ == "__main__":
    labels, counts = parse_logs()
    plot_distribution(labels, counts)
