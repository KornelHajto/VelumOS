# 🕸️ VelumOS: Distributed Operating System Kernel

> **A fault-tolerant, decentralized operating system kernel for turning resource-constrained devices into a cohesive compute mesh.**



---

### 📖 What is this?
**VelumOS** is not just a simulator anymore; it is a **distributed kernel library**. 

It allows you to link a lightweight C++ library (`velum_core.cpp`) into any application to instantly grant it **distributed computing superpowers**. It transforms a collection of dumb nodes (like ESP32s, Raspberry Pis, or Linux terminals) into a single, fault-tolerant supercomputer.

It handles **discovery, load balancing, task checkpointing, and failure recovery** automatically in the background, exposing a simple API to the developer.

---

### ⚡ Core Capabilities

* **🧩 Scatter-Gather Parallelism:** Automatically splits large jobs (e.g., 4 Billion Pi iterations) into chunks and distributes them across *all* available workers simultaneously.
* **💾 Checkpointing & Resume:** Workers send incremental progress updates. If a node crashes at 90%, the system detects it and reschedules *only the remaining 10%* to a new node. No work is lost.
* **🗣️ Gossip Mesh:** Leaderless architecture. Nodes discover each other via TCP gossip and form a mesh without central configuration.
* **🛡️ Self-Healing:** The kernel maintains a ledger of pending tasks. If a worker vanishes, the kernel seamlessly re-assigns its workload to the next best candidate.

---

### 🚀 The Developer Experience (API)

VelumOS hides the complexity of sockets and threads behind a clean C++ API.

**1. Initialize the Kernel:**
```cpp
// Starts the background mesh engine on Port 8001
velum_init(1, 8001); 

2. Spawn a Distributed Task:
C++

// Instantly scatters 1 Billion iterations across the cluster
velum_spawn(velum::TaskOp::COMPUTE_PI, 1000000000);

The kernel handles the rest: finding workers, splitting the job, tracking progress, handling crashes, and aggregating the result.
🎮 How to Run the Cluster

    Compile the Project:
    Bash

make clean && make

Launch the Mesh: Open 4 terminal tabs and run:
Bash

./node 1
./node 2
./node 3
./node 4

Unleash the Power: In Node 1's terminal, type:
Bash

    pi 400000000

    Watch the magic:

        Node 1 detects 3 workers.

        It splits the job into 133,333,333 chunks.

        Nodes 2, 3, and 4 start crunching in parallel.

        If you kill Node 2 mid-task, Node 1 will detect the crash and re-assign its remaining work to Node 3 or 4 automatically.

📊 Performance & Scaling

VelumOS demonstrates near-linear speedup for compute-bound tasks.
Workers	Execution Time (2B iters)	Speedup Factor
1 Node	60.1s	1.0x
2 Nodes	31.5s	1.91x
4 Nodes	16.2s	3.70x
8 Nodes	8.5s	7.10x

(Benchmarks run on an 8-core host machine)
🛠️ Architecture

    Language: C++17 (No external dependencies)

    Concurrency: std::thread for the Kernel, select() for I/O multiplexing.

    Networking: Raw Berkeley Sockets (<sys/socket.h>).

    Fault Tolerance: Active Heartbeats + Ledger Re-verification.

🔮 Future Roadmap

    [ ] Port velum_core.cpp to ESP-IDF (FreeRTOS) for physical microcontroller deployment.

    [ ] Implement dynamic code loading (WebAssembly/Lua) for arbitrary task execution.
