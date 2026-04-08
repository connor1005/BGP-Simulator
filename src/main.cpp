#include <iostream>
#include <vector>
#include <string>
#include "../include/ASGraph.hpp"
#include "../include/BGP.hpp"

int main() {
    ASGraph graph;

    // 1. Load the Graph
    std::cout << "Loading CAIDA dataset...\n";
    // Point this to your test file or the full CAIDA dataset
    graph.parse_caida_file("data/test_caida.txt"); 

    // 2. Validate Topology
    // Requirement: Must error out if provider/customer cycles exist [cite: 84, 220]
    std::cout << "Running cycle detection...\n";
    graph.check_cycles();

    // 3. Flatten the Graph
    // Requirement: Turn DAG into vector of vectors for propagation ranks [cite: 139, 140]
    std::cout << "Flattening the graph...\n";
    graph.flatten_graph();

    std::cout << "\n--- Seeding & Propagation ---\n";

    // 4. Seed Announcements
    // Seeding at the bottom to test Up/Across [cite: 145]
    graph.seed_announcement(4, "8.8.8.0/24");
    // Seeding at the top to test Down [cite: 145]
    graph.seed_announcement(1, "1.1.1.0/24");

    // 5. Propagation Phases
    // Step 1: Up (to providers) [cite: 147, 148]
    graph.propagate_up();

    // Step 2: Across (to peers - strictly one hop) [cite: 147, 157]
    graph.propagate_across();

    // Step 3: Down (to customers) [cite: 147, 161]
    graph.propagate_down();

    // 6. Verify Results (Local RIBs)
    // Requirement: ASes store one entry per prefix in the local RIB [cite: 127, 129]
    std::cout << "\n--- Final Routing Tables (Local RIB) ---\n";
    for (const auto& [asn, node] : graph.as_nodes) {
        auto bgp = dynamic_cast<BGP*>(node->bgp_policy.get());
        if (bgp && !bgp->local_rib.empty()) {
            std::cout << "AS " << asn << " RIB:\n";
            for (const auto& [prefix, ann] : bgp->local_rib) {
                std::cout << "  - Prefix: " << prefix 
                          << " | Path: ";
                for (size_t i = 0; i < ann.as_path.size(); ++i) {
                    std::cout << ann.as_path[i] << (i == ann.as_path.size() - 1 ? "" : "-");
                }
                std::cout << " | NextHop: " << ann.next_hop_asn << "\n";
            }
        }
    }

    return 0;
}
