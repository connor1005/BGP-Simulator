#include <iostream>
#include "../include/ASGraph.hpp"
#include "../include/BGP.hpp"

int main() {
    ASGraph graph;
    graph.parse_caida_file("data/test_caida.txt");
    
    // 1. Load ROV Defense
    // AS 2 will now use the ROV policy instead of standard BGP
    graph.load_rov_asns("data/rov_asns.txt");

    graph.check_cycles();
    graph.flatten_graph();

    // 2. The Hijack Scenario
    std::string hijacked_prefix = "1.2.0.0/16";

    // Legitimate announcement from AS 1
    graph.seed_announcement(1, hijacked_prefix);

    // HIJACK: AS 4 announces the same prefix, but it's marked invalid
    // We need a slight manual tweak here to simulate the 'invalid' flag
    auto hijacker_bgp = dynamic_cast<BGP*>(graph.as_nodes[4]->bgp_policy.get());
    Announcement hijack_ann;
    hijack_ann.prefix = hijacked_prefix;
    hijack_ann.as_path = {4};
    hijack_ann.next_hop_asn = 4;
    hijack_ann.recv_relationship = Relationship::ORIGIN;
    hijack_ann.rov_invalid = true; // This is the "fake" announcement [cite: 200, 210]
    hijacker_bgp->local_rib[hijacked_prefix] = hijack_ann;

    // 3. Run Simulation
    graph.propagate_up();
    graph.propagate_across();
    graph.propagate_down();

    // 4. Verify ROV Logic
    std::cout << "\n--- ROV Test Results ---\n";
    for (uint32_t asn : {2, 3}) {
        auto bgp = dynamic_cast<BGP*>(graph.as_nodes[asn]->bgp_policy.get());
        if (bgp && bgp->local_rib.count(hijacked_prefix)) {
            const auto& ann = bgp->local_rib[hijacked_prefix];
            std::cout << "AS " << asn << " (ROV: " << (asn == 2 ? "YES" : "NO") << ") ";
            std::cout << "chose path: ";
            for (uint32_t p : ann.as_path) std::cout << p << " ";
            std::cout << (ann.rov_invalid ? "[HIJACKED!]" : "[SAFE]") << "\n";
        }
    }

    return 0;
}
