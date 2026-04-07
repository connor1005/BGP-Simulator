#include <cstdint>
#include <vector>
#include <memory>

class Policy;

class AS {
public:
    uint32_t asn;
    int propagation_rank = -1; 

    // Using weak pointers for edges to prevent memory leaks from bidirectional peers
    std::vector<std::weak_ptr<AS>> providers;
    std::vector<std::weak_ptr<AS>> customers;
    std::vector<std::weak_ptr<AS>> peers;
    
    // We will eventually need a pointer to a concrete Policy object 
    // std::unique_ptr<Policy> bgp_policy;

    // Constructor
    AS(uint32_t id) : asn(id) {}
};
