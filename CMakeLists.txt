# PSn00bSDK example CMake script
# (C) 2021 spicyjpeg - MPL licensed

cmake_minimum_required(VERSION 3.21)

project(
	blockpuzzle-PSn00bSDK
	LANGUAGES    C CXX ASM
	VERSION      1.0.0
	DESCRIPTION  "blockpuzzle PSX"
	HOMEPAGE_URL "TBD"
)

psn00bsdk_add_executable(blockpuzzle-psx GPREL
	src/main.c
	src/program.c
	src/spi.c
)

psn00bsdk_add_cd_image(
	iso      # Target name
	blockpuzzle-psx # Output file name (= blockpuzzle-psx.bin + blockpuzzle-psx.cue)
	iso.xml  # Path to config file
	DEPENDS blockpuzzle-psx system.cnf
)

# my addition
set(MRUBY_LIB_PATH "/media/psf/Home/Repos/parallels-ubuntu/PSX/mruby/build/playstation/lib")
find_library(MRUBYLIB
	libmruby.a
	PATHS "${MRUBY_LIB_PATH}"
	NO_DEFAULT_PATH
)

find_library(MRUBYCORELIB
	libmruby_core.a
	PATHS "${MRUBY_LIB_PATH}"
	NO_DEFAULT_PATH
)

if(MRUBYLIB AND MRUBYCORELIB)
	message(STATUS "mruby libraries found: ${MRUBYLIB}")
	target_link_libraries(blockpuzzle-psx PRIVATE "${MRUBYLIB}")
	target_link_libraries(blockpuzzle-psx PRIVATE "${MRUBYCORELIB}")
	target_include_directories(blockpuzzle-psx PRIVATE "/media/psf/Home/Repos/parallels-ubuntu/PSX/mruby/build/playstation/include")

	# no math.h so no float
	target_compile_definitions(blockpuzzle-psx PRIVATE MRB_NO_FLOAT)
	# no FILE
	target_compile_definitions(blockpuzzle-psx PRIVATE MRB_NO_STDIO)
else()
	message(WARNING "mruby library not found")
endif()
