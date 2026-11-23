# Cobra : raccourcis. Les outils et le banc se construisent avec CMake
# (build/), la ROM avec un assembleur compatible asm68k.

BUILD := build
DATA := $(BUILD)/data
ROM := rom/cobra.bin

# L'assembleur : asm68k (SN 68k) ou clownassembler_asm68k, le premier
# trouvé ; sinon celui que construit « make asm » dans build/.
ASM68K ?= $(firstword $(foreach a,asm68k clownassembler_asm68k,\
	$(shell command -v $(a) 2>/dev/null)) \
	$(wildcard $(BUILD)/clownassembler/clownassembler_asm68k))

.PHONY: all tools data rom asm test sim clean rebuild

all: tools data

tools:
	@cmake -S . -B $(BUILD) >/dev/null
	@cmake --build $(BUILD) --target cobraconv romsum cobrasim

# La conversion de la zone de démo : build/data/
data: tools
	@mkdir -p $(DATA)
	$(BUILD)/cobraconv demo/zone/demo.vox demo/zone/cubes.txt demo/assets \
		demo/assets/heros.bmp $(DATA)

# La ROM : rom/cobra.bin (elle est aussi dans le dépôt, déjà assemblée)
rom: data
	@test -n "$(ASM68K)" || { echo "Pas d'assembleur : make asm (clownassembler, compatible asm68k)"; exit 1; }
	$(ASM68K) /p rom/cobra.asm,$(ROM)
	$(BUILD)/romsum $(ROM)

# Construit clownassembler (licence AGPL, compatible asm68k) dans build/ ;
# il lui faut son sous-module clowncommon
asm:
	@test -d $(BUILD)/clownassembler-src || git clone --depth 1 \
		https://github.com/Clownacy/clownassembler.git $(BUILD)/clownassembler-src
	git -C $(BUILD)/clownassembler-src submodule update --init --depth 1
	cmake -S $(BUILD)/clownassembler-src -B $(BUILD)/clownassembler \
		-DCMAKE_BUILD_TYPE=Release >/dev/null
	cmake --build $(BUILD)/clownassembler --target clownassembler_asm68k

test: data
	tests/run_tests.sh

# La ROM sur le banc, avec la promenade de démo : build/cobrasim.bmp
sim: data
	$(BUILD)/cobrasim $(ROM) demo/promenade.txt $(BUILD)/cobrasim.bmp

clean:
	rm -rf $(BUILD)/CMakeFiles $(BUILD)/CMakeCache.txt $(DATA)

rebuild: clean all
