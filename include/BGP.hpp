#ifndef BGP_HPP
#define BGP_HPP

#include <unordered_map>
#include <vector>
#include <string>
#include "Announcement.hpp"

// Abstract base class for routing policies
class Policy {
public:
    virtual ~Policy() = default;
};

// Concrete BGP Policy
class BGP : public Policy {
private:
	bool is_better(const Announcement& new_ann, const Announcement& current_ann);
public:
    std::unordered_map<std::string, Announcement> local_rib;
    std::unordered_map<std::string, std::vector<Announcement>> received_queue;

    void process_queue(uint32_t current_asn);
};

#endif
