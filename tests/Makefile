# Test suites
#
#   make unit           build + run unit tests (no driver needed)
#   make integration    build + run driver tests (module must be loaded)
#   make                build both

CXX      ?= g++
# -Wno-sign-compare: CHECK_EQ compares int32_t fields with C enum constants
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wno-sign-compare -O0 -g
CPPFLAGS += -I. -I../app/include -I../include

APP := ../app/src

UNIT_SRCS   := unit_tests.cpp $(APP)/AlertManager.cpp $(APP)/Display.cpp $(APP)/Logger.cpp
DRIVER_SRCS := driver_tests.cpp $(APP)/ParkingSensor.cpp $(APP)/SensorException.cpp

all: unit_tests driver_tests

unit_tests: $(UNIT_SRCS) test_framework.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $(UNIT_SRCS)

driver_tests: $(DRIVER_SRCS) test_framework.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $(DRIVER_SRCS)

unit: unit_tests
	./unit_tests

integration: driver_tests
	./driver_tests

clean:
	rm -f unit_tests driver_tests *.tmp

.PHONY: all unit integration clean
