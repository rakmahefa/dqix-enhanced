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
SDL_LIBS := -L$(PAYLOAD_ESC)/lib -Wl,-rpath,'$$ORIGIN/payload/lib' -lSDL2
else
SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS := $(shell pkg-config --libs sdl2)
endif

IMGUI_ESC := $(call escape_spaces,$(IMGUI_DIR))
OBJ_DIR := $(BUILD_DIR)/obj

LAUNCHER_SRC := launcher.cpp
IMGUI_SRCS := \
	$(IMGUI_DIR)/imgui.cpp \
	$(IMGUI_DIR)/imgui_draw.cpp \
	$(IMGUI_DIR)/imgui_tables.cpp \
	$(IMGUI_DIR)/imgui_widgets.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_sdlrenderer2.cpp

# Object files: one per source, so unchanged ImGui sources are not recompiled.
LAUNCHER_OBJ := $(OBJ_DIR)/launcher.o
IMGUI_OBJS := $(patsubst $(IMGUI_DIR)/%.cpp,$(OBJ_DIR)/imgui/%.o,$(IMGUI_SRCS))
OBJS := $(LAUNCHER_OBJ) $(IMGUI_OBJS)
DEPS := $(OBJS:.o=.d)

TARGET := $(BUILD_DIR)/dqix-launcher
TARGET_ESC := $(call escape_spaces,$(TARGET))

.PHONY: all clean check
all: $(TARGET_ESC)

check:
	@test -f "$(IMGUI_DIR)/imgui.h" || { echo "Dear ImGui submodule is missing. Run: git submodule update --init --recursive"; exit 1; }
	@pkg-config --exists sdl2 || test -n "$(PAYLOAD_ABS)" || { echo "SDL2 development files are missing."; exit 1; }

$(TARGET_ESC): $(OBJS) | check
	@mkdir -p "$(BUILD_DIR)"
	$(CXX) $(CXXFLAGS) -o "$@" $(OBJS) $(SDL_LIBS) -pthread
	@chmod 0755 "$@"

# Pattern rules: compile each source to its own object with dependency tracking.
$(OBJ_DIR)/launcher.o: launcher.cpp | check
	@mkdir -p "$(OBJ_DIR)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SDL_CFLAGS) -MMD -MP -c -o "$@" launcher.cpp

$(OBJ_DIR)/imgui/%.o: $(IMGUI_DIR)/%.cpp | check
	@mkdir -p "$(dir $@)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SDL_CFLAGS) -MMD -MP -c -o "$@" "$<"

-include $(DEPS)

clean:
	rm -rf "$(BUILD_DIR)"
