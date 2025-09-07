cmake_minimum_required(VERSION 3.24)
project(PSC LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set C standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Compiler flags
if(MSVC)
    add_compile_options("/bigobj")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W0 /D_USE_MATH_DEFINES")
    set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fpermissive -D_USE_MATH_DEFINES")
    if(APPLE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    elseif(UNIX)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -Wno-error=stringop-overflow")
    endif()
endif()

# Debug/Release flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g3 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")

# Find required packages
find_package(OpenMP)
find_package(Threads REQUIRED)

# Include directories
include_directories(src)
include_directories(CoACD/public)
include_directories(CoACD/src)
include_directories(CoACD/3rd/cdt/CDT)

# Use pre-built CoACD libraries
find_library(COACD_LIBRARY 
    NAMES coacd libcoacd.a
    PATHS ${CMAKE_SOURCE_DIR}/CoACD/build
    NO_DEFAULT_PATH
)

find_library(COACD_SHARED_LIBRARY 
    NAMES _coacd lib_coacd.so
    PATHS ${CMAKE_SOURCE_DIR}/CoACD/build
    NO_DEFAULT_PATH
)

if(NOT COACD_LIBRARY OR NOT COACD_SHARED_LIBRARY)
    message(FATAL_ERROR "CoACD libraries not found. Please build CoACD first by running 'cd CoACD && mkdir -p build && cd build && cmake .. -DWITH_3RD_PARTY_LIBS=ON -DCMAKE_CXX_STANDARD=17 -DTBB_TEST=OFF && make coacd main _coacd -j$(nproc)'")
endif()

# Create imported target for CoACD static library
add_library(coacd_static STATIC IMPORTED)
set_target_properties(coacd_static PROPERTIES
    IMPORTED_LOCATION ${COACD_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/CoACD/public;${CMAKE_SOURCE_DIR}/CoACD/src;${CMAKE_SOURCE_DIR}/CoACD/3rd/cdt/CDT"
)

# Create imported target for CoACD shared library
add_library(coacd_shared SHARED IMPORTED)
set_target_properties(coacd_shared PROPERTIES
    IMPORTED_LOCATION ${COACD_SHARED_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/CoACD/public"
)

# Link CoACD with required libraries
if(OpenMP_CXX_FOUND)
    target_link_libraries(coacd_static INTERFACE OpenMP::OpenMP_CXX)
endif()
target_link_libraries(coacd_static INTERFACE Threads::Threads)

# PSC source files
set(PSC_SRCS
    src/main.c
    src/stl_parser.c
    src/topology_evaluator.c
    src/PSC_model_inspector.c
    src/mesh_adjacency.c
    src/convex_decomposition_simple.c
    src/coacd_wrapper.cpp
)

# PSC test source files
set(PSC_TEST_SRCS
    tests/PSC_tests.c
)

# PSC CoACD convex decomposition main
set(PSC_COACD_SRCS
    src/convex_main_simple.c
    src/stl_parser.c
    src/topology_evaluator.c
    src/PSC_model_inspector.c
    src/convex_decomposition_simple.c
    src/coacd_wrapper.cpp
)

# Main PSC executable
add_executable(parametric_slicer ${PSC_SRCS})
target_link_libraries(parametric_slicer PRIVATE coacd_static)

# PSC model inspector test
add_executable(PSC_model_inspector ${PSC_TEST_SRCS} src/stl_parser.c src/topology_evaluator.c src/PSC_model_inspector.c src/convex_decomposition_simple.c)
target_link_libraries(PSC_model_inspector PRIVATE coacd_static)

# CoACD convex decomposition program
add_executable(convex_decomposition_coacd ${PSC_COACD_SRCS})
target_link_libraries(convex_decomposition_coacd PRIVATE coacd_static)

# Find OpenGL and GLFW
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# Link OpenGL libraries and CoACD shared library to the CoACD program
target_link_libraries(convex_decomposition_coacd PRIVATE 
    OpenGL::GL 
    glfw 
    GLEW::GLEW
    coacd_shared
)

# Create output directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_output)

# Custom target to run all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_BINARY_DIR}/PSC_model_inspector ${CMAKE_SOURCE_DIR}/A.stl
    COMMAND ${CMAKE_BINARY_DIR}/convex_decomposition_coacd ${CMAKE_SOURCE_DIR}/A.stl 0.05
    DEPENDS PSC_model_inspector convex_decomposition_coacd
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running all PSC tests and CoACD convex decomposition programs"
)

# Install targets
install(TARGETS parametric_slicer PSC_model_inspector convex_decomposition_coacd
    RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "PSC Configuration Summary:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  C standard: ${CMAKE_C_STANDARD}")
message(STATUS "  OpenMP found: ${OpenMP_CXX_FOUND}")
message(STATUS "  CoACD integration: Enabled")
