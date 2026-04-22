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

        if (as_nodes.find(asn0) == as_nodes.end()) {
            as_nodes[asn0] = std::make_shared<AS>(asn0);
	    as_nodes[asn0]->bgp_policy = std::make_unique<BGP>();
        }
        if (as_nodes.find(asn1) == as_nodes.end()) {
            as_nodes[asn1] = std::make_shared<AS>(asn1);
	    as_nodes[asn1]->bgp_policy = std::make_unique<BGP>();
        }

        if (relationship == -1) {
            as_nodes[asn0]->customers.push_back(as_nodes[asn1]);
            as_nodes[asn1]->providers.push_back(as_nodes[asn0]);
        } else if (relationship == 0) {
            as_nodes[asn0]->peers.push_back(as_nodes[asn1]);
            as_nodes[asn1]->peers.push_back(as_nodes[asn0]);
        }
    }
}

bool ASGraph::has_provider_cycle(uint32_t asn, std::unordered_map<uint32_t, int>& states) {
	states[asn] = 1; 
	auto as_node = as_nodes[asn];

    	for (auto& weak_provider : as_node->providers) {
        	if (auto provider = weak_provider.lock()) { 
            		uint32_t provider_asn = provider->asn;

            		if (states[provider_asn] == 1) {
                	std::cerr << "CYCLE DETECTED: Cycle involves AS " << provider_asn << "\n";
                	return true; 
            		}
            		if (states[provider_asn] == 0) {
                		if (has_provider_cycle(provider_asn, states)) {
                    			return true;
                		}
            		}
        	}
    	}

    	states[asn] = 2; 
    	return false;
}

void ASGraph::check_cycles() {
    std::unordered_map<uint32_t, int> states;
    
    for (const auto& pair : as_nodes) {
        states[pair.first] = 0;
    }

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
    if (memo.find(as_node->asn) != memo.end()) {
        return memo[as_node->asn];
    }

    if (as_node->customers.empty()) {
        memo[as_node->asn] = 0;
        as_node->propagation_rank = 0;
        return 0;
    }

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

    for (auto& [asn, as_node] : as_nodes) {
        int rank = calculate_rank(as_node, memo);
        if (rank > highest_rank) {
            highest_rank = rank;
        }
    }

    ranks.resize(highest_rank + 1);

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
    seed_ann.as_path = {origin_asn};       
    seed_ann.next_hop_asn = origin_asn;    
    seed_ann.recv_relationship = Relationship::ORIGIN;

    auto bgp = dynamic_cast<BGP*>(as_nodes[origin_asn]->bgp_policy.get());
    if (bgp) {
        bgp->local_rib[prefix] = seed_ann;
        std::cout << "Seeded prefix " << prefix << " at AS " << origin_asn << "\n";
    }
}

void ASGraph::propagate_up() {
    for (size_t rank = 0; rank < ranks.size(); ++rank) {
        
        for (auto& as_node : ranks[rank]) {
            auto bgp = dynamic_cast<BGP*>(as_node->bgp_policy.get());
            if (bgp) {
                bgp->process_queue(as_node->asn);
            }
        }

        for (auto& as_node : ranks[rank]) {
            auto bgp = dynamic_cast<BGP*>(as_node->bgp_policy.get());
            if (bgp && !bgp->local_rib.empty()) {
                
                for (auto& weak_provider : as_node->providers) {
                    if (auto provider = weak_provider.lock()) {
                        auto prov_bgp = dynamic_cast<BGP*>(provider->bgp_policy.get());
                        if (prov_bgp) {
                            
                            for (const auto& [prefix, ann] : bgp->local_rib) {
				    if (ann.recv_relationship == Relationship::ORIGIN || ann.recv_relationship == Relationship::CUSTOMER) {
                                Announcement out_ann = ann;
                               	out_ann.next_hop_asn = as_node->asn; 
                                out_ann.recv_relationship = Relationship::CUSTOMER;
                                
                                prov_bgp->received_queue[prefix].push_back(out_ann);
				    }
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
    for (auto& [asn, as_node] : as_nodes) {
        auto bgp = dynamic_cast<BGP*>(as_node->bgp_policy.get());
        if (bgp && !bgp->local_rib.empty()) {
            
            for (auto& weak_peer : as_node->peers) {
                if (auto peer = weak_peer.lock()) {
                    auto peer_bgp = dynamic_cast<BGP*>(peer->bgp_policy.get());
                    if (peer_bgp) {
                        for (const auto& [prefix, ann] : bgp->local_rib) {
				if (ann.recv_relationship == Relationship::ORIGIN || 
        ann.recv_relationship == Relationship::CUSTOMER) {
                            Announcement out_ann = ann;
                            
                            out_ann.next_hop_asn = as_node->asn;
                            out_ann.recv_relationship = Relationship::PEER;
                            
                            peer_bgp->received_queue[prefix].push_back(out_ann);
				}
                        }
                    }
                }
            }
        }
    }

    for (auto& [asn, as_node] : as_nodes) {
        auto bgp = dynamic_cast<BGP*>(as_node->bgp_policy.get());
        if (bgp) {
            bgp->process_queue(asn);
        }
    }
    std::cout << "Propagation Across phase complete.\n";
}

void ASGraph::propagate_down() {
    for (int rank = (int)ranks.size() - 1; rank >= 0; --rank) {
        
        for (auto& as_node : ranks[rank]) {
            auto bgp = dynamic_cast<BGP*>(as_node->bgp_policy.get());
            if (bgp && !bgp->local_rib.empty()) {
                
                for (auto& weak_cust : as_node->customers) {
                    if (auto customer = weak_cust.lock()) {
                        auto cust_bgp = dynamic_cast<BGP*>(customer->bgp_policy.get());
                        if (cust_bgp) {
                            for (const auto& [prefix, ann] : bgp->local_rib) {
                                Announcement out_ann = ann;
                                
				out_ann.next_hop_asn = as_node->asn;
                                out_ann.recv_relationship = Relationship::PROVIDER;
                                
                                
                                cust_bgp->received_queue[prefix].push_back(out_ann);
                            }
                        }
                    }
                }
            }
        }

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
            as_nodes[rov_asn]->bgp_policy = std::make_unique<ROV>();
        }
    }
}


void ASGraph::write_ribs_csv(const std::string& filename) {
    std::ofstream file(filename);
    file << "asn,prefix,as_path\n";

    for (const auto& [asn, node] : as_nodes) {
        auto bgp = dynamic_cast<BGP*>(node->bgp_policy.get());
        if (bgp) {
            for (const auto& [prefix, ann] : bgp->local_rib) {
                file << asn << "," << prefix << ",\"(";
                
                for (size_t i = 0; i < ann.as_path.size(); ++i) {
                    file << ann.as_path[i];
                    if (ann.as_path.size() > 1) {
                        if (i < ann.as_path.size() - 1) {
                            file << ", ";
                        }
                    } else {
                        file << ",";
                    }
                }
                
                file << ")\"\n";
            }
        }
    }
}



void ASGraph::load_announcements(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    std::getline(file, line); 

    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        
        uint32_t asn;
        std::string prefix;
        bool invalid;

        std::getline(ss, token, ','); asn = std::stoul(token);
        std::getline(ss, token, ','); prefix = token;
        std::getline(ss, token, ','); 
	invalid = (token.find("True") != std::string::npos ||
			token.find("true") != std::string::npos ||
			token.find("1") != std::string::npos);

        if (as_nodes.count(asn)) {
            Announcement ann;
            ann.prefix = prefix;
            ann.as_path = {asn};
            ann.next_hop_asn = asn;
            ann.recv_relationship = Relationship::ORIGIN;
            ann.rov_invalid = invalid;

            auto bgp = dynamic_cast<BGP*>(as_nodes[asn]->bgp_policy.get());
            if (bgp) bgp->local_rib[prefix] = ann;
        }
    }
}

// --- WEB-SAFE OVERLOADS ---

void ASGraph::parse_caida_file_from_string(const std::string& csv_content) {
    std::stringstream file(csv_content);
    std::string line;
    while (std::getline(file, line)) {
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

        if (as_nodes.find(asn0) == as_nodes.end()) {
            as_nodes[asn0] = std::make_shared<AS>(asn0);
	    as_nodes[asn0]->bgp_policy = std::make_unique<BGP>();
        }
        if (as_nodes.find(asn1) == as_nodes.end()) {
            as_nodes[asn1] = std::make_shared<AS>(asn1);
	    as_nodes[asn1]->bgp_policy = std::make_unique<BGP>();
        }

        if (relationship == -1) {
            as_nodes[asn0]->customers.push_back(as_nodes[asn1]);
            as_nodes[asn1]->providers.push_back(as_nodes[asn0]);
        } else if (relationship == 0) {
            as_nodes[asn0]->peers.push_back(as_nodes[asn1]);
            as_nodes[asn1]->peers.push_back(as_nodes[asn0]);
        }
    }
}


void ASGraph::load_rov_asns_from_string(const std::string& csv_content) {
    std::stringstream file(csv_content);
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        uint32_t rov_asn = std::stoul(line);
        
        if (as_nodes.count(rov_asn)) {
            as_nodes[rov_asn]->bgp_policy = std::make_unique<ROV>();
        }
    }

}

void ASGraph::load_announcements_from_string(const std::string& csv_content) {
    std::stringstream file(csv_content);
    std::string line;
    std::getline(file, line);
     while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        
        uint32_t asn;
        std::string prefix;
        bool invalid;

        std::getline(ss, token, ','); asn = std::stoul(token);
        std::getline(ss, token, ','); prefix = token;
        std::getline(ss, token, ','); 
	invalid = (token.find("True") != std::string::npos ||
			token.find("true") != std::string::npos ||
			token.find("1") != std::string::npos);

        if (as_nodes.count(asn)) {
            Announcement ann;
            ann.prefix = prefix;
            ann.as_path = {asn};
            ann.next_hop_asn = asn;
            ann.recv_relationship = Relationship::ORIGIN;
            ann.rov_invalid = invalid;

            auto bgp = dynamic_cast<BGP*>(as_nodes[asn]->bgp_policy.get());
            if (bgp) bgp->local_rib[prefix] = ann;
        }
    }

}

std::string ASGraph::get_rib_for_asn_as_string(uint32_t target_asn) {
    std::stringstream ss;
    if (as_nodes.count(target_asn)) {
        auto bgp = dynamic_cast<BGP*>(as_nodes[target_asn]->bgp_policy.get());
        if (bgp) {
            ss << "asn,prefix,as_path\n";
            for (const auto& [prefix, ann] : bgp->local_rib) {
                ss << target_asn << "," << prefix << ",\"(";
                for (size_t i = 0; i < ann.as_path.size(); ++i) {
                    // Match the EXACT comma formatting we fixed earlier
                    ss << ann.as_path[i];
                    if (ann.as_path.size() > 1) {
                        if (i < ann.as_path.size() - 1) ss << ", ";
                    } else {
                        ss << ",";
                    }
                }
                ss << ")\"\n";
            }
        }
    } else {
        ss << "Error: ASN not found in topology.";
    }
    return ss.str();
}
