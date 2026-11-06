ROOT := $(abspath ../..)
IMGUI_DIR := $(ROOT)/third_party/imgui
BUILD_DIR ?= $(ROOT)/build/launcher
PAYLOAD ?=
CXX ?= c++
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -Wno-unused-parameter
CPPFLAGS += -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends

ifneq ($(strip $(PAYLOAD)),)
PAYLOAD_ABS := $(abspath $(PAYLOAD))
SDL_CFLAGS := -I$(PAYLOAD_ABS)/include/SDL2
SDL_LIBS := -L$(PAYLOAD_ABS)/lib -Wl,-rpath,'$$ORIGIN/payload/lib' -lSDL2
else
SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS := $(shell pkg-config --libs sdl2)
endif

SOURCES := \
	launcher.cpp \
	$(IMGUI_DIR)/imgui.cpp \
	$(IMGUI_DIR)/imgui_draw.cpp \
	$(IMGUI_DIR)/imgui_tables.cpp \
	$(IMGUI_DIR)/imgui_widgets.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_sdlrenderer2.cpp

TARGET := $(BUILD_DIR)/dqix-launcher

.PHONY: all clean check
all: $(TARGET)

check:
	@test -f "$(IMGUI_DIR)/imgui.h" || { echo "Dear ImGui submodule is missing. Run: git submodule update --init --recursive"; exit 1; }
	@pkg-config --exists sdl2 || test -n "$(PAYLOAD_ABS)" || { echo "SDL2 development files are missing."; exit 1; }

$(TARGET): $(SOURCES) | check
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SDL_CFLAGS) -o "$@" $(SOURCES) $(SDL_LIBS) -pthread
	@chmod 0755 "$@"

clean:
	rm -f "$(TARGET)"
