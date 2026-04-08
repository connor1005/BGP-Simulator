# BGP Simulator (C++ Engine)

A high-performance C++ simulator designed to model Internet routing, valley-free path propagation, and Route Origin Validation (ROV). This project reads CAIDA topology files, processes BGP announcements, and calculates the resulting Routing Information Bases (RIBs) for Autonomous Systems.

## Features
* **Valley-Free Routing:** Implements standard BGP export policies (Customer -> Provider, Peer -> Peer, etc.) using a rank-based graph traversal.
* **Route Origin Validation (ROV):** Polymorphic architecture allows specific AS nodes to deploy ROV to detect and drop hijacked prefixes.
* **Cycle Detection:** Automatically detects and handles provider/customer cycles in the CAIDA topology.

## Speed Optimizations & Design Decisions
To ensure maximum execution speed for large datasets (e.g., the `many` dataset), the following optimizations were implemented:
* **Efficient Memory Allocation:** Graph nodes and relationships are managed using smart pointers (`std::shared_ptr`) to prevent memory leaks and reduce deep copy overhead.
* **Optimized String Passing:** Strings (like AS paths and prefixes) are passed by constant reference (`const std::string&`) throughout the propagation loops to eliminate expensive memory allocations.
* **Hash Maps for Lookups:** Standard ordered `std::map` structures were replaced with `std::unordered_map` where possible, reducing graph traversal lookups from $O(\log n)$ to $O(1)$.
* **Polymorphic Fast-Pathing:** The ROV logic is built as a subclass that overrides the standard BGP queue processing, meaning standard nodes don't waste CPU cycles checking ROV flags if they haven't deployed the defense.
