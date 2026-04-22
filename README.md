# BGP Simulator (C++ Engine)

A high-performance C++ simulator designed to model Internet routing, valley-free path propagation, and Route Origin Validation (ROV). This project reads CAIDA topology files, processes BGP announcements, and calculates the resulting Routing Information Bases (RIBs) for Autonomous Systems.

## Features
* **Valley-Free Routing:** Implements standard BGP export policies (Customer -> Provider, Peer -> Peer, etc.) using a rank-based graph traversal.
* **Route Origin Validation (ROV):** Polymorphic architecture allows specific AS nodes to deploy ROV to detect and drop hijacked prefixes.
* **Cycle Detection:** Automatically detects and handles provider/customer cycles in the CAIDA topology.

## Speed Optimizations & Design Decisions

To ensure maximum execution speed and stability for large datasets, the following C++ optimizations were implemented:

* **Cyclic-Safe Memory Management:** Graph relationships are modeled using a strict `std::shared_ptr` / `std::weak_ptr` paradigm. Nodes own themselves, while relationship edges (providers, peers, customers) use `weak_ptr.lock()` during traversal. This prevents cyclical reference memory leaks common in dense BGP topologies without requiring manual garbage collection.
* **Hash Maps for Lookups:** Standard ordered `std::map` structures were replaced with `std::unordered_map` for the core `as_nodes` registry, reducing graph traversal and node lookups from $O(\log n)$ to $O(1)$.
* **Reference-Based Iteration:** During the intensive `propagate` phases, the Routing Information Bases (RIBs) are traversed using structured binding with constant references (`const auto& [prefix, ann]`). This prevents expensive and continuous reallocation of prefix string keys when building outbound queues.
* **Polymorphic Fast-Pathing:** The BGP policy engine is abstracted (`bgp_policy.get()`). This allows the ROV logic to be built as a subclass that overrides the standard queue processing, meaning standard BGP nodes don't waste CPU cycles checking ROV flag conditionals if they haven't explicitly deployed the defense.
