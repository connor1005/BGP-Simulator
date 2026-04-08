CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -g

# Updated source files list including BGP.cpp
SRCS = src/main.cpp src/ASGraph.cpp src/BGP.cpp

# Name of the final executable
TARGET = bgp_sim

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)
