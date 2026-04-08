#ifndef ANNOUNCEMENT_HPP
#define ANNOUNCEMENT_HPP

#include <string>
#include <vector>
#include <cstdint>

enum class Relationship {
    PROVIDER = 0,
    PEER = 1,
    CUSTOMER = 2,
    ORIGIN = 3
};

struct Announcement {
    std::string prefix;
    std::vector<uint32_t> as_path;
    uint32_t next_hop_asn;
    Relationship recv_relationship;
    bool rov_invalid = false;
};

#endif
