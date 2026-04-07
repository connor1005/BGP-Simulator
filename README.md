# BGP-Simulator

## Project Directory Structure

```text
bgp-simulator/
├── data/                  # CAIDA datasets and CSVs (Added to .gitignore)
├── include/               # Header files (.hpp)
│   └── AS.hpp             # AS node class blueprint
├── src/                   # Source files (.cpp)
│   ├── AS.cpp             # AS node method implementations
│   └── main.cpp           # Main entry point of the simulator
├── tests/                 # Testing suite
│   └── test_graph.cpp     # Graph and cycle-detection tests
├── README.md              # Design decisions and optimization notes
└── Makefile               # Compilation instructions
