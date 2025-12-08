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

# Benchmark Definitions
# We only benchmark tasks that support the 'scatter' interface (start, end)
BENCHMARKS = [
    {
        "name": "Wasm Pi (Monte Carlo)",
        # Function: pi_hits(start, end). Workload: 200 Million iterations
        "command_template": "scatter tests/pi.wasm pi_hits 0 2000000000",
        "file": "tests/pi.wasm"
    },
    {
        "name": "Wasm Primes (Brute Force)",
        # Function: count_primes(start, end). Workload: 0 to 5 000,000
        # (Prime checking is expensive, so number is smaller than Pi)
        "command_template": "scatter tests/primes.wasm count_primes 0 50000000",
        "file": "tests/primes.wasm"
    }
]

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
    log_path = f"{LOG_DIR}/node_{monitor_node}.log"
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        if os.path.exists(log_path):
            with open(log_path, "r") as f:
                content = f.read()
                # Matches standard "FINISHED" or Wasm "Returned"
                if "FINISHED!" in content or "Returned:" in content:
                    return time.time() - start_time
        time.sleep(0.1)
    return -1 

def run_suite():
    setup()
    
    # Test Config: 2 nodes (1 worker) up to 8 nodes (7 workers)
    node_counts = [2, 3, 4, 5, 6, 7, 8]
    
    all_results = {} # Store results for all benchmarks

    for bench in BENCHMARKS:
        test_name = bench['name']
        cmd = bench['command_template']
        wasm_file = os.path.join(PROJECT_ROOT, bench['file'])

        # Skip if file doesn't exist (avoid crashing)
        if not os.path.exists(wasm_file):
            print(f"\n⚠️  Skipping {test_name}: File {bench['file']} not found.")
            continue

        print(f"\n🚀 BENCHMARK: {test_name}")
        print(f"   Command: {cmd}")
        print("="*50)
        
        bench_data = {} # Results for this specific test

        for n in node_counts:
            num_workers = n - 1
            print(f"\n[Run] {num_workers} Workers (Total Nodes: {n})")
            
            start_cluster(n)
            
            # Wasm engine needs a bit more time to sync/load on startup
            sync_time = 3 + (n * 0.2)
            time.sleep(sync_time)
            
            send_command(1, cmd)
            
            duration = wait_for_completion(1, timeout=180)
            
            if duration != -1:
                print(f"   ✅ Finished in {duration:.4f} seconds")
                bench_data[n] = duration
            else:
                print("   ❌ Timed out!")
                bench_data[n] = None
                
            cleanup()
            time.sleep(1) 

        all_results[test_name] = bench_data

    return all_results

def plot_all(all_results):
    print("\n📊 Generating Graphs...")
    
    for test_name, data in all_results.items():
        x = [n for n, t in data.items() if t is not None]
        y = [data[n] for n in x]
        
        if not x: continue

        base_time = y[0] 
        speedup = [base_time / t for t in y]
        workers_count = [n - 1 for n in x]

        plt.figure(figsize=(10, 8))
        
        # Plot 1: Time
        plt.subplot(2, 1, 1)
        plt.plot(workers_count, y, marker='o', linestyle='-', color='#FF6B6B', linewidth=2, label='Time (s)')
        plt.title(f'VelumOS: {test_name}')
        plt.ylabel('Seconds')
        plt.grid(True, linestyle='--', alpha=0.6)
        
        # Plot 2: Speedup
        plt.subplot(2, 1, 2)
        plt.plot(workers_count, speedup, marker='s', linestyle='-', color='#4ECDC4', linewidth=2, label='Actual Speedup')
        plt.plot(workers_count, workers_count, linestyle=':', color='gray', alpha=0.7, label='Ideal')
        
        plt.xlabel('Number of Worker Nodes')
        plt.ylabel('Speedup Factor')
        plt.legend()
        plt.grid(True, linestyle='--', alpha=0.6)
        
        # Save as filename based on test name
        safe_name = test_name.replace(" ", "_").replace("(", "").replace(")", "").lower()
        output_file = os.path.join(TESTS_DIR, f"graph_{safe_name}.png")
        plt.savefig(output_file)
        print(f"✨ Saved graph: {output_file}")
        
    plt.show()

if __name__ == "__main__":
    try:
        results = run_suite()
        plot_all(results)
    except KeyboardInterrupt:
        cleanup()
