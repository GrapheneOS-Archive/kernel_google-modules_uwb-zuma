cmake_minimum_required(VERSION 3.13)

project(mac)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../tools/cmake)

option(ENABLE_TEST_COVERAGE "Enable test coverage" OFF)

enable_testing()

add_library(mac
	ca.c
	default_region.c
	fproc.c
	fproc_broken.c
	fproc_multi.c
	fproc_rx.c
	fproc_stopped.c
	fproc_tx.c
	frame.c
	main.c
	ops.c
	schedule.c
	simple_ranging_region.c
	regions.c)
target_include_directories(mac PUBLIC include include/compat include/stub)
target_compile_options(mac PRIVATE
	-Wall
	-Wdeclaration-after-statement
	-Wframe-larger-than=2048
	-Wimplicit-fallthrough
	-Wundef
	-Wvla
	-Wno-unused-but-set-variable
	-Wno-unused-const-variable
	-Wno-unused-value
	-Werror)

if(ENABLE_TEST_COVERAGE)
	include(Coverage)
	target_coverage(mac)
endif()
include(Sanitize)
target_sanitize(mac)

add_subdirectory(utest)
