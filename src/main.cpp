#include <iostream>
#include "../include/ASGraph.hpp"
#include "../include/BGP.hpp"

int main() {
    ASGraph graph;

    std::cout << "Step 1: Parsing AS Graph...\n";
    graph.parse_caida_file("data/CAIDAASGraphCollector_2025.10.15.txt"); 

    graph.check_cycles();

    std::cout << "Step 2: Loading ROV deployments...\n";
    graph.load_rov_asns("data/rov_asns.csv");

    std::cout << "Step 3: Flattening graph layers...\n";
    graph.flatten_graph();

    std::cout << "Step 4: Seeding announcements...\n";
    graph.load_announcements("data/anns.csv");

    std::cout << "Step 5: Starting propagation engine...\n";
    graph.propagate_up();
    graph.propagate_across();
    graph.propagate_down();

    std::cout << "Step 6: Exporting results to ribs.csv...\n";
    graph.write_ribs_csv("ribs.csv");

    std::cout << "Simulation complete.\n";
    return 0;
}
