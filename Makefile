CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread -O2 -Isrc
LDFLAGS  := -pthread

SRC_DIR  := src
OBJ_DIR  := obj

SRV_SRCS := $(filter-out $(SRC_DIR)/cliente.cpp, $(shell find $(SRC_DIR) -name '*.cpp'))
CLI_SRCS := $(SRC_DIR)/cliente.cpp $(wildcard $(SRC_DIR)/comun/*.cpp)

SRV_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRV_SRCS))
CLI_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(CLI_SRCS))

TARGETS  := servidor cliente

all: $(TARGETS)

servidor: $(SRV_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

cliente: $(CLI_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGETS)
	rm -rf $(OBJ_DIR)

.PHONY: all clean servidor cliente