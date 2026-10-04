# Builds the user-space dashboard: app/parking_monitor
#
#   make          build
#   make clean    remove build artefacts

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2
CPPFLAGS += -Iinclude -I../include

TARGET := parking_monitor
SRCS   := $(wildcard src/*.cpp)
OBJS   := $(patsubst src/%.cpp,build/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/%.o: src/%.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p build

clean:
	rm -rf build $(TARGET)

-include $(OBJS:.o=.d)

.PHONY: all clean
