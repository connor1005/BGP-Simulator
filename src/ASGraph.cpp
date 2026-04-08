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
	    as_nodes[asn0]->bgp_policy = std::make_unique<BGP>();
        }
        if (as_nodes.find(asn1) == as_nodes.end()) {
            as_nodes[asn1] = std::make_shared<AS>(asn1);
	    as_nodes[asn1]->bgp_policy = std::make_unique<BGP>();
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

int ASGraph::calculate_rank(std::shared_ptr<AS> as_node, std::unordered_map<uint32_t, int>& memo) {
    // If we already calculated this node's rank, return it immediately
    if (memo.find(as_node->asn) != memo.end()) {
        return memo[as_node->asn];
    }

    // Base case: If the node has no customers, its rank is 0
    if (as_node->customers.empty()) {
        memo[as_node->asn] = 0;
        as_node->propagation_rank = 0;
        return 0;
    }

    // Recursive case: Rank is 1 + the max rank of all its customers
    int max_customer_rank = -1;
    for (auto& weak_cust : as_node->customers) {
        if (auto cust = weak_cust.lock()) {
            int cust_rank = calculate_rank(cust, memo);
            if (cust_rank > max_customer_rank) {
                max_customer_rank = cust_rank;
            }
        }
    }

    int final_rank = max_customer_rank + 1;
    memo[as_node->asn] = final_rank;
    as_node->propagation_rank = final_rank;
    
    return final_rank;
}

void ASGraph::flatten_graph() {
    std::unordered_map<uint32_t, int> memo;
    int highest_rank = 0;

    // 1. Calculate the rank for every node
    for (auto& [asn, as_node] : as_nodes) {
        int rank = calculate_rank(as_node, memo);
        if (rank > highest_rank) {
            highest_rank = rank;
        }
    }

    // 2. Resize our 2D vector to hold all the ranks we found
    ranks.resize(highest_rank + 1);

    // 3. Place each node into its corresponding rank bucket
    for (auto& [asn, as_node] : as_nodes) {
        ranks[as_node->propagation_rank].push_back(as_node);
    }

    std::cout << "Graph flattened successfully into " << ranks.size() << " ranks.\n";
}

void ASGraph::seed_announcement(uint32_t origin_asn, const std::string& prefix) {
    if (as_nodes.find(origin_asn) == as_nodes.end()) {
        std::cerr << "Error: Origin AS " << origin_asn << " not found in graph.\n";
        return;
    }

    Announcement seed_ann;
    seed_ann.prefix = prefix;
    seed_ann.as_path = {origin_asn};       // The path starts here
    seed_ann.next_hop_asn = origin_asn;    // It came from itself
    seed_ann.recv_relationship = Relationship::ORIGIN; // Top priority!

    // Store it directly into the origin node's local RIB
    auto bgp = dynamic_cast<BGP*>(as_nodes[origin_asn]->bgp_policy.get());
    if (bgp) {
        bgp->local_rib[prefix] = seed_ann;
        std::cout << "Seeded prefix " << prefix << " at AS " << origin_asn << "\n";
    }
}

void ASGraph::propagate_up() {
    // Traverse from rank 0 (bottom) up to the highest rank (top)
    for (size_t rank = 0; rank < ranks.size(); ++rank) {
        
        // 1. Process Phase: Every AS in this rank processes its queue
        for (auto& as_node : ranks[rank]) {
            auto bgp = dynamic_cast<BGP*>(as_node->bgp_policy.get());
            if (bgp) {
                // This handles conflict resolution and stores best routes in the Local RIB
                bgp->process_queue(as_node->asn);
            }
        }

        // 2. Sending Phase: Every AS in this rank sends its stored routes to its providers
        for (auto& as_node : ranks[rank]) {
            auto bgp = dynamic_cast<BGP*>(as_node->bgp_policy.get());
            if (bgp && !bgp->local_rib.empty()) {
                
                // Loop through all of this node's providers
                for (auto& weak_provider : as_node->providers) {
                    if (auto provider = weak_provider.lock()) {
                        auto prov_bgp = dynamic_cast<BGP*>(provider->bgp_policy.get());
                        if (prov_bgp) {
                            
                            // Send every route we have in our RIB to the provider
                            for (const auto& [prefix, ann] : bgp->local_rib) {
                                Announcement out_ann = ann;
                                
                                // To our provider, we are their customer. 
                                out_ann.recv_relationship = Relationship::CUSTOMER;
                                out_ann.next_hop_asn = as_node->asn;
                                
                                // Drop it in the provider's queue for them to process next rank
                                prov_bgp->received_queue[prefix].push_back(out_ann);
                            }
                        }
                    }
                }
            }
        }
    }
    std::cout << "Propagation Up phase complete.\n";
}

void ASGraph::propagate_across() {
    // Step 1: ALL ASes send their RIBs to their peers' received_queues
    for (auto& [asn, as_node] : as_nodes) {
        auto bgp = dynamic_cast<BGP*>(as_node->bgp_policy.get());
        if (bgp && !bgp->local_rib.empty()) {
            
            for (auto& weak_peer : as_node->peers) {
                if (auto peer = weak_peer.lock()) {
                    auto peer_bgp = dynamic_cast<BGP*>(peer->bgp_policy.get());
                    if (peer_bgp) {
                        for (const auto& [prefix, ann] : bgp->local_rib) {
                            Announcement out_ann = ann;
                            
                            // Relationship is PEER, next hop is the current ASN
                            out_ann.recv_relationship = Relationship::PEER;
                            out_ann.next_hop_asn = asn;
                            
                            peer_bgp->received_queue[prefix].push_back(out_ann);
                        }
                    }
                }
            }
        }
    }

    // Step 2: ALL ASes process their queues (after everyone has finished sending)
    for (auto& [asn, as_node] : as_nodes) {
        auto bgp = dynamic_cast<BGP*>(as_node->bgp_policy.get());
        if (bgp) {
            bgp->process_queue(asn);
        }
    }
    std::cout << "Propagation Across phase complete.\n";
}

void ASGraph::propagate_down() {
    // Traverse from the highest rank down to rank 0
    for (int rank = (int)ranks.size() - 1; rank >= 0; --rank) {
        
        // 1. Sending Phase: ASes in this rank send RIBs to customers
        for (auto& as_node : ranks[rank]) {
            auto bgp = dynamic_cast<BGP*>(as_node->bgp_policy.get());
            if (bgp && !bgp->local_rib.empty()) {
                
                for (auto& weak_cust : as_node->customers) {
                    if (auto customer = weak_cust.lock()) {
                        auto cust_bgp = dynamic_cast<BGP*>(customer->bgp_policy.get());
                        if (cust_bgp) {
                            for (const auto& [prefix, ann] : bgp->local_rib) {
                                Announcement out_ann = ann;
                                
                                // Relationship is CUSTOMER, next hop is current ASN
                                out_ann.recv_relationship = Relationship::PROVIDER;
                                out_ann.next_hop_asn = as_node->asn;
                                
                                cust_bgp->received_queue[prefix].push_back(out_ann);
                            }
                        }
                    }
                }
            }
        }

        // 2. Process Phase: The rank BELOW must process what they just received
        if (rank > 0) {
            for (auto& lower_node : ranks[rank - 1]) {
                auto lower_bgp = dynamic_cast<BGP*>(lower_node->bgp_policy.get());
                if (lower_bgp) {
                    lower_bgp->process_queue(lower_node->asn);
                }
            }
        }
    }
    std::cout << "Propagation Down phase complete.\n";
}

void ASGraph::load_rov_asns(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    if (!file.is_open()) return;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        uint32_t rov_asn = std::stoul(line);
        
        if (as_nodes.count(rov_asn)) {
            // Replace the standard BGP brain with an ROV brain 
            as_nodes[rov_asn]->bgp_policy = std::make_unique<ROV>();
        }
    }
}
