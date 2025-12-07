import subprocess
import time
import os
import re
import matplotlib.pyplot as plt
import sys

# --- CONFIGURATION ---
TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(TESTS_DIR, ".."))
NODE_BIN = f"{PROJECT_ROOT}/node"
LOG_DIR = os.path.join(TESTS_DIR, "benchmark_logs")

# Workload: 2 Billion (Adjust if too slow for your PC)
WORKLOAD = 2000000000 

NODES = {}

def setup():
    print(f"🛠️  [Setup] Compiling VelumOS...")
    subprocess.run(["make", "clean", "&&", "make"], cwd=PROJECT_ROOT, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not os.path.exists(LOG_DIR): os.makedirs(LOG_DIR)
    cleanup()

def cleanup():
    for pid in NODES.values(): pid.terminate()
    subprocess.run(["pkill", "-f", "node"], stdout=subprocess.DEVNULL)
    NODES.clear()

def start_cluster(n_nodes):
    print(f"   -> Launching {n_nodes} nodes...", end=" ", flush=True)
    for i in range(1, n_nodes + 1):
        log_file = open(f"{LOG_DIR}/node_{i}.log", "w")
        proc = subprocess.Popen([NODE_BIN, str(i)], stdin=subprocess.PIPE, stdout=log_file, stderr=log_file, text=True, bufsize=0, cwd=PROJECT_ROOT)
        NODES[i] = proc
    print("Done.")

def send_command(node_id, cmd):
    if node_id in NODES:
        NODES[node_id].stdin.write(cmd + "\n")
        NODES[node_id].stdin.flush()

def wait_for_completion(monitor_node, timeout=120):
    """Watches log file for completion keyword."""
    log_path = f"{LOG_DIR}/node_{monitor_node}.log"
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        if os.path.exists(log_path):
            with open(log_path, "r") as f:
                content = f.read()
                # FIXED KEYWORD: Matches what velum_core.cpp actually prints
                if "FINISHED!" in content or "FINAL SUCCESS" in content:
                    return time.time() - start_time
        time.sleep(0.1)
    return -1 

def run_benchmark():
    results = {} 
    setup()
    
    # Test range: 2 nodes (1 worker) to 9 nodes (8 workers)
    node_counts = [2, 4, 8, 16]
    
    print("\n🚀 STARTING PERFORMANCE BENCHMARK")
    print(f"   Task: Calculate PI ({WORKLOAD} iterations)")
    print("="*50)

    for n in node_counts:
        num_workers = n - 1
        print(f"\n[Test Case] {num_workers} Workers (Total Nodes: {n})")
        
        start_cluster(n)
        
        # Wait for mesh sync
        sync_time = 3 + (n * 0.2)
        time.sleep(sync_time)
        
        # Send Job
        send_command(1, f"pi {WORKLOAD}")
        
        # Measure
        duration = wait_for_completion(1, timeout=120)
        
        if duration != -1:
            print(f"   ✅ Finished in {duration:.4f} seconds")
            results[n] = duration
        else:
            print("   ❌ Timed out! (Check logs in benchmark_logs/)")
            results[n] = None
            
        cleanup()
        time.sleep(1) 

    return results

def plot_results(results):
    print("\n📊 Generating Graph...")
    
    x = [n for n, t in results.items() if t is not None]
    y = [results[n] for n in x]
    
    if not x:
        print("No successful data to plot.")
        return

    # Calculate Speedup (Time(1 Worker) / Time(N Workers))
    # Note: x[0] is 2 nodes (1 worker)
    base_time = y[0] 
    speedup = [base_time / t for t in y]
    
    # X-axis should label WORKERS, not Nodes, to be clearer
    workers_count = [n - 1 for n in x]

    plt.figure(figsize=(10, 8))
    
    # Plot 1: Execution Time
    plt.subplot(2, 1, 1)
    plt.plot(workers_count, y, marker='o', linestyle='-', color='#FF6B6B', linewidth=2, label='Execution Time')
    plt.title(f'VelumOS Performance: Distributed PI ({WORKLOAD} iters)')
    plt.ylabel('Time (seconds)')
    plt.grid(True, linestyle='--', alpha=0.6)
    
    # Plot 2: Speedup
    plt.subplot(2, 1, 2)
    plt.plot(workers_count, speedup, marker='s', linestyle='-', color='#4ECDC4', linewidth=2, label='Actual Speedup')
    
    # Ideal Linear Speedup Line
    plt.plot(workers_count, workers_count, linestyle=':', color='gray', alpha=0.7, label='Ideal Linear Speedup')
    
    plt.xlabel('Number of Worker Nodes')
    plt.ylabel('Speedup Factor (x)')
    plt.legend()
    plt.grid(True, linestyle='--', alpha=0.6)
    
    output_file = os.path.join(TESTS_DIR, "speedup_graph.png")
    plt.savefig(output_file)
    print(f"✨ Graph saved to: {output_file}")
    plt.show()

if __name__ == "__main__":
    try:
        data = run_benchmark()
        plot_results(data)
    except KeyboardInterrupt:
        cleanup()
