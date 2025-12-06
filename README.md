# 🕸️ VelumOS: Distributed System Simulator

> **A lightweight, leaderless mesh network simulation for resource-constrained clusters.**


---

### 📖 What is this?
This is the Linux simulator for my thesis, **VelumOS**.

Basically, it converts a set of standard terminal windows into a single, cohesive computing cluster. It simulates how low-power microcontrollers (like ESP32s or STM32s) can form a mesh, communicate via TCP, and share computational tasks without needing a central "leader" or server.

It is written in **pure C++** (no heavy frameworks) using raw Berkeley sockets, making it fast, lightweight, and easy to port to embedded hardware.

---

### ⚡ What can it do?

* **🗣️ Gossip Network:** Nodes discover each other automatically. Simply launch them, and they form a mesh.
* **🧠 Smart Load Balancing:** Every node silently gossips its health metrics (CPU load & RAM) in the background. When a task is requested, the system automatically routes it to the *least busy* node.
* **🛡️ Fault Tolerance:** If a node crashes or disconnects, the cluster detects it immediately and removes it from the scheduling pool. No freezing, no timeouts.
* **🚀 Remote Execution:** You can issue a command on *any* node (e.g., `add 50 100`), and it might be executed by a completely different node in the cluster based on available resources.

---

### 🎮 How to Run

1.  **Compile the Project:**
    ```bash
    make clean && make
    ```

2.  **Start the Cluster:**
    Open 3 (or more) separate terminal tabs and run:
    ```bash
    ./node 1
    ./node 2
    ./node 3
    ```

3.  **Interact:**
    Wait a few seconds for the nodes to sync up (you won't see much output—they are chatting in the background). Then, type a command in **Node 1's** terminal:
    ```bash
    add 10 20
    ```
    *Watch the logs! Node 1 will analyze the cluster, find the best worker (e.g., Node 2), offload the task, and display the result.*

    **Supported Commands:**
    * `add <a> <b>`  (e.g., `add 50 100`)
    * `sub <a> <b>`  (e.g., `sub 20 5`)
    * `mul <a> <b>`  (e.g., `mul 6 7`)
    * `div <a> <b>`  (e.g., `div 10 2`)

---

### 🛠️ Under the Hood

* **Language:** C++ (Standard 11/17)
* **Networking:** Raw TCP Sockets (`<sys/socket.h>`)
* **Concurrency:** Non-blocking Event Loop (`select()`)
* **Protocol:** Custom Binary-Packed Structs (Zero parsing overhead)

### 🔮 Future Work
This simulator serves as the architectural proof-of-concept. The next phase of VelumOS involves porting this C++ logic directly to **ESP32 hardware** to demonstrate a physical distributed microcontroller system.

