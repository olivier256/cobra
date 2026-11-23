#!/bin/sh
# Tests de Cobra (make test) : depuis la racine du dépôt, après make data.
set -e
cd "$(dirname "$0")/.."
BUILD=${BUILD:-build}

cmake -S . -B "$BUILD" >/dev/null
cmake --build "$BUILD" --target test_scene test_rom romsum >/dev/null

# Tests compilés avec AddressSanitizer et UBSan (voir CMakeLists.txt) ;
# les outils ne libèrent pas toute leur mémoire en sortant : pas de
# recherche de fuites.
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}

echo "== test_scene"
"$BUILD/test_scene"

# La ROM du dépôt doit être celle que donnent les sources : réassemblée si
# un assembleur est là (voir Makefile)
ASM=
for a in asm68k clownassembler_asm68k; do
    [ -z "$ASM" ] && ASM=$(command -v "$a" 2>/dev/null) || true
done
if [ -z "$ASM" ] && [ -x "$BUILD/clownassembler/clownassembler_asm68k" ]; then
    ASM="$BUILD/clownassembler/clownassembler_asm68k"
fi
echo "== rom/cobra.bin"
if [ -n "$ASM" ]; then
    "$ASM" /p rom/cobra.asm,"$BUILD/cobra_test.bin" >/dev/null
    "$BUILD/romsum" "$BUILD/cobra_test.bin"
    if cmp -s "$BUILD/cobra_test.bin" rom/cobra.bin; then
        echo "réassemblée : identique"
    else
        echo "ÉCHEC : rom/cobra.bin n'est pas celle des sources (make rom)"
        exit 1
    fi
else
    echo "pas d'assembleur : ROM du dépôt prise telle quelle (make asm pour vérifier)"
fi

echo "== test_rom"
"$BUILD/test_rom"
