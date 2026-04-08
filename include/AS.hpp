#include <cstdint>
#include <vector>
#include <memory>
#include "BGP.hpp"

class Policy;

class AS {
public:
    uint32_t asn;
    int propagation_rank = -1; 

    std::vector<std::weak_ptr<AS>> providers;
    std::vector<std::weak_ptr<AS>> customers;
    std::vector<std::weak_ptr<AS>> peers;
    
    std::unique_ptr<Policy> bgp_policy;

    AS(uint32_t id) : asn(id) {}
};
