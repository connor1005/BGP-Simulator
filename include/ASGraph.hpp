#ifndef ASGRAPH_HPP
#define ASGRAPH_HPP

#include <unordered_map>
#include <memory>
#include <string>
#include "AS.hpp"

class ASGraph {
private:
	bool has_provider_cycle(uint32_t asn, std::unordered_map<uint32_t, int>& states);
	int calculate_rank(std::shared_ptr<AS> as_node, std::unordered_map<uint32_t, int>& memo);

public:
	std::unordered_map<uint32_t, std::shared_ptr<AS>> as_nodes;
    	std::vector<std::vector<std::shared_ptr<AS>>> ranks;

    	void parse_caida_file(const std::string& filename); 
    	void check_cycles();
    	void flatten_graph();

    	void seed_announcement(uint32_t origin_asn, const std::string& prefix);
    	void load_rov_asns(const std::string& filename);
	
    	void propagate_up();
    	void propagate_across();
    	void propagate_down();
	
	void load_announcements(const std::string& filename);
	void write_ribs_csv(const std::string& filename);

	void parse_caida_file_from_string(const std::string& csv_content);
	void load_rov_asns_from_string(const std::string& csv_content);
	void load_announcements_from_string(const std::string& csv_content);
	std::string get_rib_for_asn_as_string(uint32_t target_asn);
};

#endif
