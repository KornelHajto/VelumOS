# 🕸️ VelumOS: Distributed Wasm Microkernel

> **A fault-tolerant, decentralized operating system overlay kernel for turning resource-constrained devices into a cohesive compute mesh.**



---

### 📖 What is this?
**VelumOS** is a **distributed kernel library** designed for high-performance edge computing. 

It allows you to link a lightweight C++ microkernel (`velum_core.cpp`) into any application to instantly grant it **distributed computing superpowers**. It transforms a collection of dumb nodes (like ESP32s, Raspberry Pis, or Linux terminals) into a single, fault-tolerant supercomputer.

Unlike traditional distributed systems that require specific hardcoded logic, VelumOS is a **Pure Microkernel**. It provides the infrastructure (Discovery, Scheduling, Fault Tolerance) while all application logic is executed securely via **WebAssembly (Wasm)** sandboxes.

---

### ⚡ Core Capabilities

* **🧩 Scatter-Gather Parallelism:** Automatically splits massive workloads (e.g., 4 Billion iterations) into chunks and distributes them across *all* available workers simultaneously.
* **📦 Universal Runtime (WebAssembly):** Executes arbitrary binary code (`.wasm`). Developers can write distributed logic in **C, Rust, or Go**, compile it once, and the VelumOS kernel executes it safely in a sandbox on any node.
* **💾 Checkpointing & Resume:** Workers send incremental progress updates. If a node crashes at 90%, the system detects it and reschedules *only the remaining 10%* to a new node. No work is lost.
* **🗣️ Gossip Mesh:** Leaderless architecture. Nodes discover each other via UDP beacons and form a mesh without central configuration.
* **🛡️ Self-Healing:** The kernel maintains a ledger of pending tasks. If a worker vanishes, the kernel seamlessly re-assigns its workload to the next best candidate.

---

### 🚀 The Developer Experience (API)

VelumOS hides the complexity of sockets, threads, and VMs behind a clean C++ API.

**1. Initialize the Kernel:**
```cpp
// Starts the background mesh engine & Wasm3 VM on Port 8001
velum_init(1, 8001); 

```
2. Spawn a Dynamic Task (WebAssembly):
```cpp

// Sends a binary file to the cluster to execute custom logic.
// Automatically partitions the range [0-100000] across all workers.
velum_spawn_wasm_distributed("tests/primes.wasm", "count_primes", 0, 100000);
```
🎮 How to Run the Cluster

    Compile the Project:

```sh

make clean && make

```
Launch the Mesh: Open 4 terminal tabs and run:

```sh

./node 1
./node 2
./node 3
./node 4

```
Watch as they automatically discover each other via UDP Broadcast.

Unleash the Power:

Scenario A: Dynamic Code Execution

```sh

# Run a custom C function compiled to Wasm
wasm tests/sum.wasm sum 10 20 30

```

Node 1 packages the arguments, sends the binary to Node 2, executes it in a sandbox, and returns 60.

Scenario B: Distributed MapReduce

```sh

    # Count prime numbers in a range using arbitrary Wasm logic
    scatter tests/primes.wasm count_primes 0 500000

```
    VelumOS automatically partitions the range and scatters the Wasm binary to the entire cluster.

📊### 📊 Performance & Scaling

VelumOS is designed to minimize overhead, achieving near-linear (and sometimes superlinear) speedup for compute-intensive tasks.

#### 1. Compute-Bound Scaling (Monte Carlo Pi)
* **Workload:** 2 Billion Iterations.
* **Result:** The system demonstrates perfect linear scaling. Doubling the node count halves the execution time, proving that the **Overlay Kernel** introduces negligible overhead for heavy tasks.

| Workers | Execution Time | Speedup Factor |
| :--- | :--- | :--- |
| **1 Node** | 60.1s | 1.0x |
| **2 Nodes** | 31.5s | 1.91x |
| **4 Nodes** | 16.2s | 3.70x |
| **8 Nodes** | 8.5s | 7.10x |

<img width="1000" height="800" alt="graph_wasm_pi_monte_carlo" src="https://github.com/user-attachments/assets/7fab0a2d-0f8b-41f5-afaa-7147dc30810a" />


---

#### 2. Resource Efficiency (Brute Force Primes)
* **Workload:** Prime number search (Trial Division).
* **Observation:** This benchmark achieved **Superlinear Speedup** (>8x speedup with 6 workers).
* **Analysis:** By distributing the workload, VelumOS allowed each node to fit its smaller working set entirely into the CPU cache (L1/L2), drastically reducing RAM latency compared to the single-node baseline. This proves that distributed memory can be faster than local memory for specific datasets.

<img width="1000" height="800" alt="graph_wasm_primes_brute_force" src="https://github.com/user-attachments/assets/1330fd8d-03f6-4224-8654-1cf3fbfa2b57" />


---

#### 3. Dynamic MapReduce (Distributed Search)
* **Workload:** `scatter tests/search.wasm search 0 100000`
* **Mechanism:** The kernel dynamically detected 3 available workers and partitioned the search space into `[0-33k]`, `[33k-66k]`, and `[66k-100k]` in real-time.
* **Result:** All nodes executed the **same Wasm binary** but processed **different data ranges** simultaneously, demonstrating true **Single Instruction, Multiple Data (SIMD)** behavior at the cluster level.

  
🛠️ Architecture

    Language: C++17 (Kernel) + C99 (Wasm3 Runtime).

    Virtual Machine: Embedded Wasm3 interpreter for safe, sandboxed execution.

    Concurrency: std::thread for the Kernel, select() for I/O multiplexing.

    Networking: Raw Berkeley Sockets (TCP/UDP).

    Fault Tolerance: Active Heartbeats + Ledger Re-verification + State Checkpointing.

🔮 Future Roadmap

    [x] Dynamic Code Loading: Support for arbitrary Wasm binaries.

    [x] Fault Tolerance: Recovery from sudden node death.

    [ ] Hardware Port: Port velum_core.cpp to ESP-IDF (FreeRTOS) for physical microcontroller deployment.

    [ ] WASI Support: Enable Wasm apps to print to stdout and access network sockets.
