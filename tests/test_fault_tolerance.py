import subprocess
import time
import os
import signal
import sys
import threading

# ==========================================
# CONFIGURATION
# ==========================================
TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(TESTS_DIR, ".."))
NODE_BIN = f"{PROJECT_ROOT}/node"
LOG_DIR = os.path.join(TESTS_DIR, "test_logs")
NODES = {}

# ANSI Colors for readability
COLORS = {
    1: "\033[94m", # Blue (Controller)
    2: "\033[92m", # Green (Worker 1)
    3: "\033[93m", # Yellow (Worker 2)
    "RESET": "\033[0m",
    "BOLD": "\033[1m"
}

def setup():
    print(f"🛠️  [Setup] Compiling C++ code in {PROJECT_ROOT}...")
    result = subprocess.run(
        ["make", "clean", "&&", "make"], 
        cwd=PROJECT_ROOT, 
        shell=True, 
        stdout=subprocess.DEVNULL, 
        stderr=subprocess.DEVNULL
    )
    
    if result.returncode != 0:
        print("❌ Compilation Failed! Check your C++ code.")
        sys.exit(1)
        
    if not os.path.exists(LOG_DIR):
        os.makedirs(LOG_DIR)
        
    subprocess.run(["pkill", "-f", "node"], stdout=subprocess.DEVNULL)

# --- REAL TIME LOGGER ---
def monitor_output(node_id, proc):
    """Reads stdout from a node, prints it to console, and saves to file."""
    log_file_path = f"{LOG_DIR}/node_{node_id}.log"
    
    # Open file in append mode (or write mode)
    with open(log_file_path, "w") as f:
        # Read line by line until process ends
        for line in iter(proc.stdout.readline, ''):
            clean_line = line.strip()
            if clean_line:
                # 1. Print to Terminal with Color
                c = COLORS.get(node_id, "\033[97m")
                r = COLORS["RESET"]
                print(f"{c}[Node {node_id}] {clean_line}{r}")
                
                # 2. Save to File (for the test logic)
                f.write(line)
                f.flush()

def start_node(node_id):
    if not os.path.isfile(NODE_BIN):
        print(f"❌ Error: Binary not found at {NODE_BIN}")
        sys.exit(1)

    # Note: We pipe stdout so we can intercept it in Python
    proc = subprocess.Popen(
        [NODE_BIN, str(node_id)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,   # Catch output
        stderr=subprocess.STDOUT, # Merge errors into stdout
        text=True,
        bufsize=0,
        cwd=PROJECT_ROOT 
    )
    NODES[node_id] = proc
    
    # Start a background thread to read/print output
    t = threading.Thread(target=monitor_output, args=(node_id, proc))
    t.daemon = True # Kill thread when main program exits
    t.start()
    
    # Small sleep to let the start message print cleanly
    time.sleep(0.1)

def send_command(node_id, command):
    if node_id in NODES:
        print(f"\n📨 [Test] Sending '{command}' to Node {node_id}...")
        try:
            NODES[node_id].stdin.write(command + "\n")
            NODES[node_id].stdin.flush()
        except OSError:
            pass

def kill_node(node_id):
    if node_id in NODES:
        print(f"\n💥 [Chaos] KILLING Node {node_id} now!")
        NODES[node_id].terminate()
        NODES[node_id].wait()
        del NODES[node_id]

def check_log(node_id, keyword):
    """Checks the log file for specific keywords."""
    log_path = f"{LOG_DIR}/node_{node_id}.log"
    if not os.path.exists(log_path): return False
    
    # We read the file because the thread writes to it
    with open(log_path, "r") as f:
        return keyword in f.read()

def cleanup():
    print(f"\n{COLORS['BOLD']}🧹 [Cleanup] Stopping cluster...{COLORS['RESET']}")
    for pid in NODES.values():
        pid.terminate()
    subprocess.run(["pkill", "-f", "node"], stdout=subprocess.DEVNULL)

def run_test():
    try:
        setup()

        start_node(1) 
        start_node(2) 
        start_node(3) 
        
        print(f"\n{COLORS['BOLD']}⏳ [Sync] Waiting 3s for mesh discovery...{COLORS['RESET']}")
        time.sleep(3)

        # Inject Task
        send_command(1, "pi 300000000") 

        print(f"\n{COLORS['BOLD']}👀 [Monitor] Watching logs to identify the worker...{COLORS['RESET']}")
        
        victim_id = None
        # Poll logs until we see who got the job
        for _ in range(20):
            if check_log(2, "Received Task"): victim_id = 2; break
            if check_log(3, "Received Task"): victim_id = 3; break
            time.sleep(0.2)
        
        if not victim_id:
            print("❌ [Fail] Task didn't start in time.")
            return

        # Let it run for a second so we see some progress bars
        time.sleep(1.5)

        kill_node(victim_id)

        print(f"\n{COLORS['BOLD']}🚑 [Recovery] Waiting 10s for Node 1 to reschedule and finish...{COLORS['RESET']}")
        time.sleep(10) 

        print("\n" + "="*40 + "\n       FAULT TOLERANCE REPORT\n" + "="*40)
        
        passes = 0
        if check_log(1, "died!"): 
            print("✅ PASS: Crash detected."); passes += 1
        else: print("❌ FAIL: Crash NOT detected.")

        if check_log(1, "Rescheduling"): 
            print("✅ PASS: Reschedule triggered."); passes += 1
        else: print("❌ FAIL: No reschedule attempt.")

        if check_log(1, "FINAL SUCCESS"): 
            print("🏆 PASS: Task completed!"); passes += 1
        else: print("❌ FAIL: Result never received.")

        print("-" * 40)
        if passes == 3: print(f"{COLORS['BOLD']}✨ SYSTEM IS FAULT TOLERANT ✨{COLORS['RESET']}")
        else: print("⚠️  SYSTEM FAILED RECOVERY TEST")

    except KeyboardInterrupt: pass
    finally: cleanup()

if __name__ == "__main__":
    run_test()
