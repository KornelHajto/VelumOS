#include <iostream>
#include <cstdint>
#include <thread>
#include <mutex>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <random>

class Node {
private:
    uint16_t id;
    std::thread worker;
    std::mutex mtx;
    std::unordered_set<uint16_t> membership;
    std::vector<Node*> peers;  // other nodes in the cluster
    bool running = true;

    // Random number generator for selecting a peer
    std::mt19937 rng;

public:
    Node(uint16_t node_id) : id(node_id), rng(std::random_device{}()) {
        membership.insert(id);
        worker = std::thread(&Node::run, this);
    }

    ~Node() {
        stop();
        join();
    }

    void add_peer(Node* peer) {
        if (peer != this) {
            peers.push_back(peer);
        }
    }

    void stop() {
        running = false;
    }

    void join() {
        if (worker.joinable())
            worker.join();
    }

    void merge_membership(const std::unordered_set<uint16_t>& other_nodes) {
        std::lock_guard<std::mutex> lock(mtx);
        membership.insert(other_nodes.begin(), other_nodes.end());
    }

    std::unordered_set<uint16_t> get_membership_copy() {
        std::lock_guard<std::mutex> lock(mtx);
        return membership;
    }

    void gossip() {
        if (peers.empty()) return;
        std::uniform_int_distribution<std::size_t> dist(0, peers.size() - 1);
        Node* target = peers[dist(rng)];
        target->merge_membership(get_membership_copy());
    }

    void run() {
        int counter = 0;
        while (running && counter < 15) {  // run 15 iterations
            std::this_thread::sleep_for(std::chrono::seconds(1));

            // Print membership
            {
                static std::mutex cout_mtx;
                std::lock_guard<std::mutex> cout_lock(cout_mtx);

                std::cout << "Node " << id << " membership { ";
                for (auto n : get_membership_copy()) std::cout << n << " ";
                std::cout << "}" << std::endl;
            }

            // Gossip to a random peer
            gossip();
            counter++;
        }
    }
};

int main() {
    // Create nodes
    Node node1(1);
    Node node2(2);
    Node node3(3);

    // Add peers
    node1.add_peer(&node2);
    node1.add_peer(&node3);

    node2.add_peer(&node1);
    node2.add_peer(&node3);

    node3.add_peer(&node1);
    node3.add_peer(&node2);

    std::cout << "All nodes are running with automatic gossip." << std::endl;

    // Wait for threads to finish
    node1.join();
    node2.join();
    node3.join();

    std::cout << "All nodes stopped cleanly." << std::endl;
    return 0;
}
