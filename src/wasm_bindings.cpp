#include <emscripten/bind.h>
#include <string>
#include "../include/ASGraph.hpp" // Adjust path if needed

using namespace emscripten;

// Notice the 4 arguments here!
std::string run_simulation_wasm(const std::string& caida_csv, 
                                const std::string& rov_csv, 
                                const std::string& announcements_csv, 
                                uint32_t target_asn) {
    ASGraph graph;
    
    // Load the data
    graph.parse_caida_file_from_string(caida_csv);
    graph.load_rov_asns_from_string(rov_csv);
    graph.flatten_graph(); 
    
    graph.load_announcements_from_string(announcements_csv);

    // Run the phases
    graph.propagate_up();
    graph.propagate_across();
    graph.propagate_down();
    
    // Output the result
    return graph.get_rib_for_asn_as_string(target_asn);
}

EMSCRIPTEN_BINDINGS(bgp_module) {
    function("runSimulation", &run_simulation_wasm);
}
