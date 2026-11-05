.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

CXX ?= g++
CPPFLAGS ?=
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?=

TARGET := mini_lexical_search
BUILD_DIR := build/make
SOURCES := $(wildcard src/*.cpp src/domain/*.cpp src/application/*.cpp \
           src/infrastructure/*.cpp src/presentation/cli/*.cpp \
           src/presentation/tui/*.cpp) third_party/ftxui/ftxui.cpp
OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
DEPFILES := $(OBJECTS:.o=.d)
FTXUI_OBJECTS := $(filter $(BUILD_DIR)/src/presentation/tui/%.o \
                 $(BUILD_DIR)/third_party/ftxui/%.o,$(OBJECTS))

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS) -pthread

# -MMD omits headers found through -isystem, including the vendored FTXUI header.
$(FTXUI_OBJECTS): third_party/ftxui/ftxui.hpp

$(BUILD_DIR)/%.o: %.cpp Makefile
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) -Iinclude -isystem third_party/ftxui $(CXXFLAGS) -std=c++20 -pthread -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)

-include $(DEPFILES)
