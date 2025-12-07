import subprocess
import time
import os
import signal
import sys

# ==========================================
# CONFIGURATION (Fixed with Absolute Paths)
# ==========================================
# Get the absolute path of the folder containing this script (tests/)
TESTS_DIR = os.path.dirname(os.path.abspath(__file__))

# The project root is one level up from tests/
PROJECT_ROOT = os.path.abspath(os.path.join(TESTS_DIR, ".."))

# The binary is inside the project root
NODE_BIN = os.path.join(PROJECT_ROOT, "node")

# Log directory inside tests/
LOG_DIR = os.path.join(TESTS_DIR, "test_logs")
NODES = {} 

def setup():
    """Compiles the project (running make in the root directory) and cleans logs."""
    print(f"🛠️  [Setup] Compiling C++ code in {PROJECT_ROOT}...")
    
    # Run make inside the project root
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
        
    # Clean up old processes
    # We filter by the binary name "node" to ensure we kill the right processes
    subprocess.run(["pkill", "-f", "node"], stdout=subprocess.DEVNULL)

def start_node(node_id):
    """Starts a node process."""
    log_file = open(f"{LOG_DIR}/node_{node_id}.log", "w")
    
    # Check if binary exists
    if not os.path.isfile(NODE_BIN):
        print(f"❌ Error: Binary not found at {NODE_BIN}")
        print(f"   (Did compilation fail?)")
        sys.exit(1)

    proc = subprocess.Popen(
        [NODE_BIN, str(node_id)],
        stdin=subprocess.PIPE,
        stdout=log_file,
        stderr=log_file,
        text=True,
        bufsize=0,
        cwd=PROJECT_ROOT # Run inside root so it behaves normally
    )
    NODES[node_id] = proc
    print(f"🚀 [Launch] Node {node_id} started (PID: {proc.pid})")

def send_command(node_id, command):
    """Writes a command to the node's stdin."""
    if node_id in NODES:
        print(f"📨 [Action] Sending '{command}' to Node {node_id}...")
        try:
            NODES[node_id].stdin.write(command + "\n")
            NODES[node_id].stdin.flush()
        except OSError:
            print(f"⚠️  [Error] Could not write to Node {node_id} (Process dead?)")

def kill_node(node_id):
    """Simulates a crash."""
    if node_id in NODES:
        print(f"💥 [Chaos] KILLING Node {node_id} now!")
        NODES[node_id].terminate()
        NODES[node_id].wait()
        del NODES[node_id]

def check_log(node_id, keyword):
    """Checks if a keyword exists in the node's log file."""
    log_path = f"{LOG_DIR}/node_{node_id}.log"
    if not os.path.exists(log_path):
        return False
        
    with open(log_path, "r") as f:
        return keyword in f.read()

def cleanup():
    """Stops all nodes."""
    print("\n🧹 [Cleanup] Stopping cluster...")
    for pid in NODES.values():
        pid.terminate()
    # Kill any remaining instances forcefully
    subprocess.run(["pkill", "-f", "node"], stdout=subprocess.DEVNULL)

def run_test():
    try:
        setup()

        # 1. Launch Cluster
        start_node(1) # Controller
        start_node(2) # Victim Worker
        start_node(3) # Backup Worker
        
        print("⏳ [Sync] Waiting 3s for mesh discovery...")
        time.sleep(3)

        # 2. Inject Heavy Task
        send_command(1, "pi 500000000") 

        # 3. Wait for assignment
        time.sleep(1)
        
        # 4. Identify the Victim
        victim_id = None
        if check_log(2, "Executing PI"): victim_id = 2
        elif check_log(3, "Executing PI"): victim_id = 3
        
        if not victim_id:
            print("❌ [Fail] Task didn't start. System too slow or busy.")
            return

        print(f"👀 [Monitor] Task detected running on Node {victim_id}")

        # 5. KILL THE VICTIM
        kill_node(victim_id)

        # 6. Wait for Recovery
        print("🚑 [Recovery] Waiting for Node 1 to detect and reschedule...")
        time.sleep(4)

        # 7. Analyze Results
        print("\n" + "="*40)
        print("       FAULT TOLERANCE REPORT")
        print("="*40)

        pass_count = 0
        
        if check_log(1, "disconnected unexpectedly"):
            print("✅ PASS: Controller detected the crash.")
            pass_count += 1
        else:
            print("❌ FAIL: Controller missed the disconnect event.")

        if check_log(1, "Rescheduling"):
            print("✅ PASS: Controller triggered reschedule logic.")
            pass_count += 1
        else:
            print("❌ FAIL: No reschedule attempt found.")

        if check_log(1, "COMPLETE"):
            print("🏆 PASS: Task successfully completed!")
            pass_count += 1
        else:
            print("❌ FAIL: Task result never received.")

        print("-" * 40)
        if pass_count == 3:
            print("✨ SYSTEM IS FAULT TOLERANT ✨")
        else:
            print("⚠️  SYSTEM FAILED RECOVERY TEST")

    except KeyboardInterrupt:
        print("\nTest Aborted.")
    finally:
        cleanup()

if __name__ == "__main__":
    run_test()
