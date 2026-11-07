ROOT := $(shell realpath -m -- "$(CURDIR)/../..")
IMGUI_DIR := $(ROOT)/third_party/imgui
BUILD_DIR ?= $(ROOT)/build/launcher
PAYLOAD ?=
CXX ?= c++
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic -Wno-unused-parameter
CPPFLAGS += -I"$(IMGUI_DIR)" -I"$(IMGUI_DIR)/backends"

ifneq ($(strip $(PAYLOAD)),)
empty :=
space := $(empty) $(empty)
escape_spaces = $(subst $(space),\$(space),$(1))
PAYLOAD_ABS := $(if $(strip $(PAYLOAD)),$(shell realpath -m -- "$(PAYLOAD)"),)
PAYLOAD_ESC := $(call escape_spaces,$(PAYLOAD_ABS))
SDL_CFLAGS := -I$(PAYLOAD_ESC)/include/SDL2
SDL_LIBS := -L$(PAYLOAD_ESC)/lib -Wl,-rpath,'$ORIGIN/payload/lib' -lSDL2
else
SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS := $(shell pkg-config --libs sdl2)
endif

IMGUI_ESC := $(call escape_spaces,$(IMGUI_DIR))
SOURCES := \
	$(IMGUI_ESC)/imgui.cpp \
	$(IMGUI_ESC)/imgui_draw.cpp \
	$(IMGUI_ESC)/imgui_tables.cpp \
	$(IMGUI_ESC)/imgui_widgets.cpp \
	$(IMGUI_ESC)/backends/imgui_impl_sdl2.cpp \
	$(IMGUI_ESC)/backends/imgui_impl_sdlrenderer2.cpp

TARGET := $(BUILD_DIR)/dqix-launcher
TARGET_ESC := $(call escape_spaces,$(TARGET))

.PHONY: all clean check
all: $(TARGET_ESC)

check:
	@test -f "$(IMGUI_DIR)/imgui.h" || { echo "Dear ImGui submodule is missing. Run: git submodule update --init --recursive"; exit 1; }
	@pkg-config --exists sdl2 || test -n "$(PAYLOAD_ABS)" || { echo "SDL2 development files are missing."; exit 1; }

$(TARGET_ESC): launcher.cpp $(SOURCES) | check
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SDL_CFLAGS) -o "$@" launcher.cpp "$(IMGUI_DIR)/imgui.cpp" "$(IMGUI_DIR)/imgui_draw.cpp" "$(IMGUI_DIR)/imgui_tables.cpp" "$(IMGUI_DIR)/imgui_widgets.cpp" "$(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp" "$(IMGUI_DIR)/backends/imgui_impl_sdlrenderer2.cpp" $(SDL_LIBS) -pthread
	@chmod 0755 "$@"

clean:
	rm -f "$(TARGET)"
