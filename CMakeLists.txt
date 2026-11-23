cmake_minimum_required(VERSION 3.13)
project(Cobra C)

set(CMAKE_C_STANDARD 99)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

# ---- Les outils sur Mac (ou Linux) ------------------------------------------

set(CONV_SOURCES tools/vox.c tools/scene.c tools/planes.c tools/tiles.c
	tools/bmpread.c)
set(CONV_INCLUDES ${CMAKE_SOURCE_DIR}/iso ${CMAKE_SOURCE_DIR}/tools)

add_executable(cobraconv tools/cobraconv.c ${CONV_SOURCES})
target_include_directories(cobraconv PRIVATE ${CONV_INCLUDES})
target_compile_options(cobraconv PRIVATE -Wall -Wextra)

add_executable(romsum tools/romsum.c)
target_compile_options(romsum PRIVATE -Wall -Wextra)

# ---- Le banc sans affichage : Musashi + modèle du VDP ------------------------
# Musashi (third_party/musashi, licence MIT) est laissé tel quel : sa
# configuration vient de sim/musashi_cobra.h, inclus avant tout. Ses tables
# d'instructions (m68kops.c) sont produites par m68kmake.

set(MUSASHI ${CMAKE_SOURCE_DIR}/third_party/musashi)
set(MUSASHI_GEN ${CMAKE_BINARY_DIR}/musashi)
add_executable(m68kmake ${MUSASHI}/m68kmake.c)
target_compile_options(m68kmake PRIVATE -w)
add_custom_command(
	OUTPUT ${MUSASHI_GEN}/m68kops.c ${MUSASHI_GEN}/m68kops.h
	COMMAND ${CMAKE_COMMAND} -E make_directory ${MUSASHI_GEN}
	COMMAND m68kmake ${MUSASHI_GEN} ${MUSASHI}/m68k_in.c
	DEPENDS m68kmake ${MUSASHI}/m68k_in.c)

set(MUSASHI_FLAGS -w -include ${CMAKE_SOURCE_DIR}/sim/musashi_cobra.h)
add_library(musashi STATIC ${MUSASHI}/m68kcpu.c ${MUSASHI}/m68kdasm.c
	${MUSASHI}/softfloat/softfloat.c ${MUSASHI_GEN}/m68kops.c)
target_include_directories(musashi PUBLIC ${MUSASHI} ${MUSASHI_GEN})
target_compile_options(musashi PRIVATE ${MUSASHI_FLAGS})
target_link_libraries(musashi PUBLIC m)

add_executable(cobrasim sim/cobrasim.c sim/md.c sim/vdp.c)
target_include_directories(cobrasim PRIVATE ${CMAKE_SOURCE_DIR}/sim)
target_compile_options(cobrasim PRIVATE -Wall -Wextra)
target_link_libraries(cobrasim PRIVATE musashi)

# ---- Tests -------------------------------------------------------------------
# Compilés avec AddressSanitizer et UBSan (Musashi aussi, dans son propre
# exemplaire) : une lecture hors d'un tableau échoue sur toutes les
# machines. -DCOBRA_SANITIZE=OFF pour s'en passer.

option(COBRA_SANITIZE "Tests avec AddressSanitizer et UBSan" ON)
set(SANITIZE_FLAGS "")
if(COBRA_SANITIZE)
	set(SANITIZE_FLAGS -fsanitize=address,undefined
		-fno-sanitize-recover=undefined -fno-omit-frame-pointer)
endif()

add_library(musashi_checked STATIC ${MUSASHI}/m68kcpu.c ${MUSASHI}/m68kdasm.c
	${MUSASHI}/softfloat/softfloat.c ${MUSASHI_GEN}/m68kops.c)
target_include_directories(musashi_checked PUBLIC ${MUSASHI} ${MUSASHI_GEN})
# Musashi décale des entiers signés vers la gauche, ce qu'UBSan signale :
# seul AddressSanitizer pour lui
set(MUSASHI_CHECK_FLAGS "")
if(COBRA_SANITIZE)
	set(MUSASHI_CHECK_FLAGS -fsanitize=address -fno-omit-frame-pointer)
endif()
target_compile_options(musashi_checked PRIVATE ${MUSASHI_FLAGS}
	${MUSASHI_CHECK_FLAGS})
target_link_libraries(musashi_checked PUBLIC m)

add_executable(test_scene tests/test_scene.c ${CONV_SOURCES})
target_include_directories(test_scene PRIVATE ${CONV_INCLUDES})

add_executable(test_rom tests/test_rom.c sim/md.c sim/vdp.c ${CONV_SOURCES})
target_include_directories(test_rom PRIVATE ${CONV_INCLUDES}
	${CMAKE_SOURCE_DIR}/sim)
target_link_libraries(test_rom PRIVATE musashi_checked)

foreach(test test_scene test_rom)
	target_compile_options(${test} PRIVATE -Wall -Wextra ${SANITIZE_FLAGS})
	target_link_options(${test} PRIVATE ${SANITIZE_FLAGS})
endforeach()
