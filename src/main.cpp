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

    std::cout << "\nFlattening the graph...\n";
    graph.flatten_graph();

    // Print out the layers to verify our logic worked
    std::cout << "\n--- Graph Layers ---\n";
    for (size_t i = 0; i < graph.ranks.size(); ++i) {
        std::cout << "Rank " << i << " ASes: ";
        for (auto& as_node : graph.ranks[i]) {
            std::cout << as_node->asn << " ";
        }
        std::cout << "\n";
    }

    return 0;
}
