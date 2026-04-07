CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -g

# Our source files
SRCS = src/main.cpp src/ASGraph.cpp

# Name of the final executable
TARGET = bgp_sim

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)
