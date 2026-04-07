#include "ASGraph.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

void ASGraph::parse_caida_file(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << "\n";
        return;
    }

    while (std::getline(file, line)) {
        // Skip comments or empty lines in the CAIDA file
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::stringstream ss(line);
        std::string token;
        uint32_t asn0, asn1;
        int relationship;

        std::getline(ss, token, '|'); 
        asn0 = std::stoul(token);
        
        std::getline(ss, token, '|'); 
        asn1 = std::stoul(token);
        
        std::getline(ss, token, '|'); 
        relationship = std::stoi(token);

        // Create the AS nodes in our map if they don't exist yet
        if (as_nodes.find(asn0) == as_nodes.end()) {
            as_nodes[asn0] = std::make_shared<AS>(asn0);
        }
        if (as_nodes.find(asn1) == as_nodes.end()) {
            as_nodes[asn1] = std::make_shared<AS>(asn1);
        }

        // Link the nodes based on the relationship value
        if (relationship == -1) {
            // asn0 is the provider, asn1 is the customer
            as_nodes[asn0]->customers.push_back(as_nodes[asn1]);
            as_nodes[asn1]->providers.push_back(as_nodes[asn0]);
        } else if (relationship == 0) {
            // Both are peers
            as_nodes[asn0]->peers.push_back(as_nodes[asn1]);
            as_nodes[asn1]->peers.push_back(as_nodes[asn0]);
        }
    }
}

bool ASGraph::has_provider_cycle(uint32_t asn, std::unordered_map<uint32_t, int>& states) {
	states[asn] = 1; 
	auto as_node = as_nodes[asn];

    // Follow all provider edges
    	for (auto& weak_provider : as_node->providers) {
        	if (auto provider = weak_provider.lock()) { 
            		uint32_t provider_asn = provider->asn;

            		if (states[provider_asn] == 1) {
                	// We hit a node currently in our path. Cycle detected!
                	std::cerr << "CYCLE DETECTED: Cycle involves AS " << provider_asn << "\n";
                	return true; 
            		}
            		if (states[provider_asn] == 0) {
                		// If unvisited, dive deeper into this provider
                		if (has_provider_cycle(provider_asn, states)) {
                    			return true;
                		}
            		}
        	}
    	}

    	// Mark as fully "Visited" and safe
    	states[asn] = 2; 
    	return false;
}

void ASGraph::check_cycles() {
    std::unordered_map<uint32_t, int> states;
    
    // Initialize all nodes to 0 (Unvisited)
    for (const auto& pair : as_nodes) {
        states[pair.first] = 0;
    }

    // Loop through every node in the graph
    for (const auto& pair : as_nodes) {
        if (states[pair.first] == 0) {
            if (has_provider_cycle(pair.first, states)) {
                std::cerr << "FATAL ERROR: Provider/Customer cycle detected in AS Graph structure. Exiting.\n";
                exit(1); 
            }
        }
    }
    
    std::cout << "Cycle check complete: Graph is a valid Directed Acyclic Graph (DAG).\n";
}
