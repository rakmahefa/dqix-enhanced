SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c
.ONESHELL:
.DEFAULT_GOAL := help
ROOT := $(CURDIR)
empty :=
space := $(empty) $(empty)
escape_spaces = $(subst $(space),\$(space),$(1))
ROM ?=
WORKSPACE ?= $(ROOT)
WORKSPACE_ABS := $(shell realpath -m -- "$(WORKSPACE)")
BUILD_DIR ?= $(WORKSPACE_ABS)/build
GAME_DIR ?= $(WORKSPACE_ABS)/game
ROM_ABS := $(if $(strip $(ROM)),$(shell realpath -m -- "$(ROM)"),)
PYTHON ?= python3
JOBS ?= $(shell getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 2)
PAYLOAD ?=
PAYLOAD_ABS := $(if $(strip $(PAYLOAD)),$(shell realpath -m -- "$(PAYLOAD)"),)
EXTRACT_DIR := $(BUILD_DIR)/extract
REGION_DIR := $(BUILD_DIR)/region
RECOMP_DIR := $(BUILD_DIR)/recomp
RECOMP7_DIR := $(BUILD_DIR)/recomp7
ifneq ($(findstring $(space),$(WORKSPACE_ABS)$(BUILD_DIR)$(GAME_DIR)),)
WORKSPACE_HASH := $(shell printf '%s' "$(ROOT)|$(WORKSPACE_ABS)|$(BUILD_DIR)|$(GAME_DIR)" | sha256sum | cut -c1-16)
OBJ_DIR := /tmp/dqix-enhanced-obj-$(shell id -u)-$(WORKSPACE_HASH)
else
OBJ_DIR := $(BUILD_DIR)/obj
endif
ROM_STAMP := $(BUILD_DIR)/.rom-sha256
REGION_FILE := $(BUILD_DIR)/region.txt
ARM9_STAMP := $(BUILD_DIR)/.arm9-recompiler.stamp
ARM7_STAMP := $(BUILD_DIR)/.arm7-recompiler.stamp

ifeq ($(OS),Windows_NT)
GAME_EXE := $(GAME_DIR)/dqix.exe
GAME_CORE := $(GAME_DIR)/dqixcore.dll
else
GAME_EXE := $(GAME_DIR)/dqix
GAME_CORE := $(GAME_DIR)/dqixcore.so
endif

ifeq ($(strip $(PAYLOAD_ABS)),)
SDL_CFLAGS ?= $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS ?= $(shell sdl2-config --libs 2>/dev/null)
else
SDL_CFLAGS ?= -I$(call escape_spaces,$(PAYLOAD_ABS))/include/SDL2
SDL_LIBS ?= -L$(call escape_spaces,$(PAYLOAD_ABS))/lib -lSDL2
endif

.PHONY: help check-rom extract region recompile-arm9 recompile-arm7 compile assemble build run test launcher screenshots release clean
help:
	printf '%s\n' 'DQIX Enhanced — build targets' '' \
	  '  make build ROM=/path/to/dqix.nds [WORKSPACE=/path] [JOBS=N]' \
	  '  make test' '  make launcher [PAYLOAD=build/payload]' \
	  '  make screenshots' '  make clean [WORKSPACE=/path]' '' \
	  'Build outputs are written to WORKSPACE/build and WORKSPACE/game.'

check-rom:
	test -n "$(ROM)" || { echo "Set ROM=/path/to/your/DQIX.nds"; exit 2; }
	test -f "$(ROM_ABS)" || { echo "ROM does not exist: $(ROM_ABS)"; exit 2; }
	$(PYTHON) "$(ROOT)/tools/check-rom.py" "$(ROM_ABS)"

extract: check-rom
	mkdir -p "$(BUILD_DIR)"
	rom_hash="$$(sha256sum "$(ROM_ABS)" | awk '{print $$1}')"
	if [[ -s "$(ROM_STAMP)" && "$$(cat "$(ROM_STAMP)")" == "$$rom_hash" && -f "$(EXTRACT_DIR)/arm9/arm9.bin" ]]; then
		echo "== Extraction is up to date"
	else
		echo "== Extracting the game's code"
		rm -rf "$(EXTRACT_DIR)" "$(RECOMP_DIR)" "$(RECOMP7_DIR)" "$(OBJ_DIR)/gen" "$(OBJ_DIR)/gen7"
		mkdir -p "$(EXTRACT_DIR)"
		"$(PYTHON)" "$(ROOT)/tools/extract_rom.py" "$(ROM_ABS)" "$(EXTRACT_DIR)"
		printf '%s\n' "$$rom_hash" > "$(ROM_STAMP)"
		rm -f "$(ARM9_STAMP)" "$(ARM7_STAMP)"
	fi

region: extract
	mkdir -p "$(REGION_DIR)"
	echo "== Mapping ROM region"
	"$(PYTHON)" "$(ROOT)/tools/region.py" "$(ROM_ABS)" "$(EXTRACT_DIR)" "$(REGION_DIR)" > "$(BUILD_DIR)/region.txt.tmp"
	region="$$(cat "$(REGION_FILE)" 2>/dev/null || true)"
	next="$$(cat "$(BUILD_DIR)/region.txt.tmp")"
	if [[ "$$region" != "$$next" ]]; then
		rm -rf "$(RECOMP_DIR)" "$(RECOMP7_DIR)" "$(OBJ_DIR)/gen" "$(OBJ_DIR)/gen7"
		rm -f "$(ARM9_STAMP)" "$(ARM7_STAMP)"
	fi
	mv "$(BUILD_DIR)/region.txt.tmp" "$(REGION_FILE)"
	echo "   Region: $$(cat "$(REGION_FILE)")"

recompile-arm9: region
	needs=0
	if [[ ! -f "$(ARM9_STAMP)" || ! -d "$(RECOMP_DIR)" ]]; then needs=1; fi
	if [[ $$needs -eq 0 ]] && find "$(ROOT)/recomp" "$(ROOT)/config" \
		"$(ROOT)/tools/region.py" "$(ROOT)/hle.txt" "$(ROOT)/interpret.txt" \
		"$(ROOT)/patches.txt" "$(ROOT)/tags.txt" "$(ROOT)/wrap.txt" \
		-type f -newer "$(ARM9_STAMP)" -print -quit | grep -q .; then needs=1; fi
	if [[ $$needs -eq 1 ]]; then
		region="$$(cat "$(REGION_FILE)")"
		if [[ "$$region" == "eur" ]]; then cfg="$(REGION_DIR)"; config="$(REGION_DIR)/arm9"
		else cfg="$(ROOT)"; config="$(ROOT)/config/usa/arm9"; fi
		echo "== Recompiling ARM9 to C"
		rm -rf "$(RECOMP_DIR)"
		"$(PYTHON)" "$(ROOT)/recomp/build.py" --extract "$(EXTRACT_DIR)" --config "$$config" \
			--out "$(RECOMP_DIR)" --hle "$$cfg/hle.txt" --interpret "$$cfg/interpret.txt" \
			--patch "$$cfg/patches.txt" --tags "$$cfg/tags.txt" --wrap "$$cfg/wrap.txt"
		touch "$(ARM9_STAMP)"
	else
		echo "== ARM9 generated code is up to date"
	fi

recompile-arm7: extract
	needs=0
	if [[ ! -f "$(ARM7_STAMP)" || ! -d "$(RECOMP7_DIR)" ]]; then needs=1; fi
	if [[ $$needs -eq 0 ]] && find "$(ROOT)/recomp/arm7.py" "$(ROOT)/hle7.txt" \
		-type f -newer "$(ARM7_STAMP)" -print -quit | grep -q .; then needs=1; fi
	if [[ $$needs -eq 1 ]]; then
		echo "== Recompiling ARM7 to C"
		rm -rf "$(RECOMP7_DIR)"
		"$(PYTHON)" "$(ROOT)/recomp/arm7.py" --extract "$(EXTRACT_DIR)" \
			--out "$(RECOMP7_DIR)" --hle "$(ROOT)/hle7.txt"
		touch "$(ARM7_STAMP)"
	else
		echo "== ARM7 generated code is up to date"
	fi

compile: recompile-arm9 recompile-arm7
	echo "== Compiling the native game and multiplayer core"
	mkdir -p "$(GAME_DIR)" "$(OBJ_DIR)"
	$(MAKE) -C "$(ROOT)" -f "$(ROOT)/build.mk" \
		GEN="$(RECOMP_DIR)" GEN7="$(RECOMP7_DIR)" OBJ="$(OBJ_DIR)" \
		FRONTEND=sdl OUT="$(GAME_EXE)" CORE="$(GAME_CORE)" \
		CC="$(CC)" CFLAGS="-O1 -w" DEFS=-DRECOMP_TRACE PIC=-fPIC \
		SDL_CFLAGS="$(SDL_CFLAGS)" SDL_LIBS="$(SDL_LIBS)" \
		$(if $(filter Windows_NT,$(OS)),OS=Windows_NT LDFLAGS="-static-libgcc -Wl,--stack,67108864",LDFLAGS="-pthread") \
		-j$(JOBS) "$(GAME_EXE)" core

assemble: compile
	echo "== Assembling the game folder"
	mkdir -p "$(GAME_DIR)/data" "$(GAME_DIR)/skins" "$(GAME_DIR)/fonts"
	rm -rf "$(GAME_DIR)/data/code"
	cp -a "$(EXTRACT_DIR)" "$(GAME_DIR)/data/code"
	if ! cmp -s "$(ROM_ABS)" "$(GAME_DIR)/data/dqix.nds"; then cp "$(ROM_ABS)" "$(GAME_DIR)/data/dqix.nds"; fi
	cp "$(ROOT)"/skins/*.png "$(ROOT)"/skins/*.txt "$(GAME_DIR)/skins/" 2>/dev/null || true
	cp "$(ROOT)"/fonts/* "$(GAME_DIR)/fonts/" 2>/dev/null || true
	for src in "$(ROOT)/extras/wfc" "$(ROOT)/wfc"; do
		[[ -d "$$src" ]] || continue
		for group in "$$src"/*/; do
			[[ -f "$${group}_list.txt" ]] || continue
			mkdir -p "$(GAME_DIR)/wfc/$$(basename "$$group")"
			cp "$$group"* "$(GAME_DIR)/wfc/$$(basename "$$group")/"
		done
	done
	if [[ "$$(cat "$(REGION_FILE)")" == "eur" ]]; then gc=YDQP; news=infomes_45.bin
	else gc=YDQE; news=infomes_47.bin; fi
	if [[ ! -f "$(GAME_DIR)/wfc/$$gc/$$news" ]]; then
		mkdir -p "$(GAME_DIR)/wfc/$$gc"
		if command -v curl >/dev/null 2>&1 && curl -fsSL --max-time 30 \
			-o "$(GAME_DIR)/wfc/$$gc/$$news" \
			"https://raw.githubusercontent.com/InternalLoss/DLS1-Files/master/$$gc/$$news"; then
			echo "   DQVC news: fetched"
		else
			rm -f "$(GAME_DIR)/wfc/$$gc/$$news"
			echo "   DQVC news: not fetched (optional)"
		fi
	fi
	if [[ -n "$(PAYLOAD_ABS)" && -f "$(PAYLOAD_ABS)/lib/libSDL2-2.0.so.0" && "$(OS)" != "Windows_NT" ]]; then
		cp "$(PAYLOAD_ABS)/lib/libSDL2-2.0.so.0" "$(GAME_DIR)/libSDL2-2.0.so.0"
	fi
	if [[ "$(OS)" == "Windows_NT" && -f /ucrt64/bin/SDL2.dll ]]; then
		cp /ucrt64/bin/SDL2.dll "$(GAME_DIR)/SDL2.dll"
	fi
	cp "$(ROOT)/tools/play.sh" "$(GAME_DIR)/play.sh"
	cp "$(ROOT)/tools/play.bat" "$(GAME_DIR)/play.bat"
	chmod 0755 "$(GAME_DIR)/play.sh"
	echo "== Build ready: $(GAME_DIR)"

build: assemble
run: build
ifeq ($(OS),Windows_NT)
	"$(GAME_DIR)/play.bat"
else
	"$(GAME_DIR)/play.sh"
endif

test:
	$(PYTHON) "$(ROOT)/tests/semantics/run.py" 2000
	$(PYTHON) -m unittest discover -s "$(ROOT)/tests/packaging" -v

launcher:
	$(MAKE) -C "$(ROOT)/tools/launcher" BUILD_DIR="$(BUILD_DIR)/launcher" PAYLOAD="$(PAYLOAD_ABS)" all

screenshots: launcher
	mkdir -p "$(BUILD_DIR)/launcher-screenshots"
	if command -v xvfb-run >/dev/null 2>&1; then
		xvfb-run -a "$(BUILD_DIR)/launcher/dqix-launcher" --preview "$(BUILD_DIR)/launcher-screenshots"
	else
		"$(BUILD_DIR)/launcher/dqix-launcher" --preview "$(BUILD_DIR)/launcher-screenshots"
	fi
	$(PYTHON) "$(ROOT)/tools/launcher/convert-screenshots.py" "$(BUILD_DIR)/launcher-screenshots"
	$(PYTHON) -m unittest discover -s "$(ROOT)/tests/packaging" -p 'test_launcher_screenshots.py' -v

release: launcher
	$(PYTHON) "$(ROOT)/tools/launcher/package.py" --repo "$(ROOT)" --launcher "$(BUILD_DIR)/launcher/dqix-launcher" \
		--payload "$(PAYLOAD_ABS)" --out "$(BUILD_DIR)/release"

clean:
	echo "Removing generated files from $(WORKSPACE_ABS)"
	rm -rf "$(BUILD_DIR)" "$(GAME_DIR)" "$(OBJ_DIR)"
