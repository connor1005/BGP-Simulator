#include <iostream>
#include "../include/ASGraph.hpp"

int main() {
    ASGraph graph;

    std::cout << "Loading CAIDA dataset...\n";
    // Point this to your dummy dataset
    graph.parse_caida_file("data/test_caida.txt"); 

    std::cout << "Total AS nodes loaded: " << graph.as_nodes.size() << "\n";

    std::cout << "Running cycle detection...\n";
    graph.check_cycles();

    return 0;
}
