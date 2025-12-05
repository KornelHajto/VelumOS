#include <iostream>
#include <cstdint>
#include <thread>
#include <mutex>
#pragma pack(1)


class Node {
private:
        uint16_t id;
        std::thread worker;
        std::mutex mtx;
public:
        Node(uint16_t node_id) : id(node_id){
            worker = std::thread(&Node::run, this);
        }

        void join(){
            if (worker.joinable()){
                worker.join();
            }
        };
        void run(){
            std::cout << "Node " << id << " is running.." << std::endl;
        };
};

int main() {
    Node node1(1);
    Node node2(2);
    Node node3(3);

    std::cout << "All nodes are running." << std::endl;

    node1.join();
    node2.join();
    node3.join();
    return 0;
}
