cmake_minimum_required(VERSION 3.9)

# Unit test
project(libkernel_test)
include(CTest)
enable_testing()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(SHOULD_FAIL_TESTS "use_after_free" "double_free")
# set(CMAKE_BUILD_TYPE Debug)
function(filter_list list_name substring)
	set(temp_list ${${list_name}})
	foreach (item ${temp_list})
		string(FIND "${item}" "${substring}" found)
		if (NOT ${found} EQUAL -1) # If the substring is found in the item
			list(REMOVE_ITEM temp_list "${item}")
		endif ()
	endforeach ()
	set(${list_name} ${temp_list} PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE TEST_SRC
	${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

# Include libkernel header
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../libkernel/include
	${CMAKE_CURRENT_SOURCE_DIR}/../libkernel/libmalloc/
	${CMAKE_CURRENT_SOURCE_DIR}/../libkernel/libmalloc/mimalloc/include/
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Include the libkernel sources
file(GLOB_RECURSE LIBKERNEL_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../libkernel/src/*.c) # adjust this path to where your source files are located
filter_list(LIBKERNEL_SRC "bpf.")
add_library(LIBKERNEL_OBJ OBJECT ${LIBKERNEL_SRC})
target_compile_definitions(LIBKERNEL_OBJ PRIVATE -DUNIT_TEST)

set(CMAKE_VERBOSE_MAKEFILE ON)
foreach (test_path ${TEST_SRC})
	get_filename_component(test_name ${test_path} NAME_WE)
	add_executable(${test_name} ${test_path})
	add_test(NAME ${test_name} COMMAND ${test_name})
	target_link_libraries(${test_name} -lpthread)
	set_property(TEST ${test_name}
		PROPERTY
		ENVIRONMENT LD_PRELOAD=/lib/libkernel.so
	)
endforeach ()

set_tests_properties(${SHOULD_FAIL_TESTS} PROPERTIES WILL_FAIL TRUE)


