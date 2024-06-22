cmake_minimum_required(VERSION 3.9)

# Project settings
project(kernel)
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)

# Compiler settings
# set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_COMPILER "/usr/bin/gcc")
set(CMAKE_CLANG_COMPILER "clang-17") # Atomic operation in bpf is only supported recent clang version
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -fPIC -fvisibility=hidden -Werror -Wno-nused-function -Wno-format -O3")
set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -felf64")
set(CMAKE_LINKER "/usr/bin/ld")
set(LIBKERNEL_INSTALL_DIR "/lib/")
set(BPF_INSTALL_DIR "/lib/sbpf/")

add_definitions(-include ${CMAKE_CURRENT_SOURCE_DIR}/include/kconfig.h)

enable_language(ASM_NASM)

# Makefile settings
set(CMAKE_VERBOSE_MAKEFILE ON)

# Set the sources and include directories of the libkernel.so and BPF objects.
file(GLOB_RECURSE LIBKERNEL_ASM "./src/*.asm" "./src/*.S" "./src/*.s")
file(GLOB_RECURSE LIBKERNEL_SRC 
	"./src/*.c"
	"./lib/libopcode/*.c"
	"./libmalloc/mimalloc/src/static.c"
)

file(GLOB_RECURSE BPF_SRC
	"./src/*.bpf.c"
)
list(REMOVE_ITEM LIBKERNEL_SRC ${BPF_SRC})
file(GLOB_RECURSE LIBKERNEL_ASM "./src/*.asm" "./src/*.S" "./src/*.s")
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/
	${CMAKE_CURRENT_SOURCE_DIR}/libmalloc/
	${CMAKE_CURRENT_SOURCE_DIR}/libmalloc/mimalloc/include/
)

# Compile the libkernel.so library OS.
add_library(
    ${PROJECT_NAME} SHARED
    ${LIBKERNEL_SRC}
    ${LIBKERNEL_ASM}
)
target_link_libraries(${PROJECT_NAME} -ldl -lbpf)

# Compile the BPF source files.
function(build_bpf prefix)
	get_filename_component(BPF_OBJECT_NAME ${prefix} NAME_WE)
	add_custom_target(${BPF_OBJECT_NAME}.bpf.o)
	add_dependencies(${PROJECT_NAME} ${BPF_OBJECT_NAME}.bpf.o)
	set_target_properties(${BPF_OBJECT_NAME}.bpf.o PROPERTIES PREFIX "")
	set_property(
		TARGET ${BPF_OBJECT_NAME}.bpf.o
		APPEND
		PROPERTY ADDITIONAL_CLEAN_FILES ${BPF_OBJECT_NAME}.bpf.o
	)
	add_custom_command(
		TARGET ${BPF_OBJECT_NAME}.bpf.o
		COMMAND ${CMAKE_CLANG_COMPILER} -g -O2 -target bpf -c -mcpu=v3  ${prefix} -o ${BPF_OBJECT_NAME}.bpf.o -I${CMAKE_CURRENT_SOURCE_DIR}/include
	)
endfunction()

foreach(bpf_files ${BPF_SRC})
	build_bpf(${bpf_files})
endforeach()

# Install the libraries to the local /lib folder.
add_custom_target(install_lib
	COMMAND cp lib${PROJECT_NAME}.so ${LIBKERNEL_INSTALL_DIR}
	COMMAND chmod 4775 ${LIBKERNEL_INSTALL_DIR}/lib${PROJECT_NAME}.so
	COMMAND mkdir -p ${BPF_INSTALL_DIR}
	COMMAND cp *.bpf.o ${BPF_INSTALL_DIR}
)

# Build external libraries
add_custom_target(build_extlib
	COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}/lib/elfutils/ &&
	autoreconf -i -f &&
	CC=\"gcc -fPIC\" ./configure --enable-maintainer-mode --disable-libdebuginfod --disable-debuginfod &&
	make -j 
)

# add_dependencies(${PROJECT_NAME} build_extlib)
