#include "BGP.hpp"

bool BGP::is_better(const Announcement& new_ann, const Announcement& current_ann){
	if (new_ann.recv_relationship != current_ann.recv_relationship) {
		return (int)new_ann.recv_relationship > (int)current_ann.recv_relationship;
	}

	if (new_ann.as_path.size() != current_ann.as_path.size()) {
		return new_ann.as_path.size() < current_ann.as_path.size();
	}

	return new_ann.next_hop_asn < current_ann.next_hop_asn;
}

void BGP::process_queue(uint32_t current_asn) {
	for (const auto& [prefix, announcements] : received_queue) {
		for (const Announcement& ann : announcements) {
			Announcement processed_ann = ann;
			processed_ann.as_path.insert(processed_ann.as_path.begin(), current_asn);
			if (local_rib.find(prefix) == local_rib.end()){
				local_rib[prefix] = processed_ann;
			}
			else if (is_better(processed_ann, local_rib[prefix])) {
					local_rib[prefix] = processed_ann;
			}
		}
	}
	received_queue.clear();
}

void ROV::process_queue(uint32_t current_asn) {
    for (auto& [prefix, announcements] : received_queue) {
        for (const Announcement& ann : announcements) {
            
            if (ann.rov_invalid) {
                continue; 
            }

            Announcement processed_ann = ann;
            processed_ann.as_path.insert(processed_ann.as_path.begin(), current_asn);

            if (local_rib.find(prefix) == local_rib.end()) {
                local_rib[prefix] = processed_ann;
            } else {
                if (is_better(processed_ann, local_rib[prefix])) {
                    local_rib[prefix] = processed_ann;
                }
            }
        }
    }
    received_queue.clear();
}
