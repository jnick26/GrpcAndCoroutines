cmake_minimum_required(VERSION 3.14)

project(GrpcPlucCoroutines)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)

#
# Google Test
#
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

#
# GRPC
#
FetchContent_Declare(
    gRPC
    GIT_REPOSITORY https://github.com/grpc/grpc
    GIT_TAG v1.51.1
)
FetchContent_MakeAvailable(gRPC)

# Since FetchContent uses add_subdirectory under the hood, we can use
# the grpc targets directly from this build.
set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_REFLECTION grpc++_reflection)
set(_PROTOBUF_PROTOC $<TARGET_FILE:protoc>)
set(_GRPC_GRPCPP grpc++)

if(CMAKE_CROSSCOMPILING)
    find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
endif()

#
# CppCoro
#
# Note:
# fork-of-the-fork is used to
# 1) Add cmake
# 2) Support compilers with experimental/coroutines instead of coroutines
#
FetchContent_Declare(
    cppcoro
    GIT_REPOSITORY https://github.com/karzhenkov/cppcoro.git
    GIT_TAG fix-cmake-coroutines
)

# set(CMAKE_CXX_STANDARD 20)
set(CXX_COROUTINES_HEADER experimantal/coroutines)
set(CXX_COROUTINES_NAMESPACE std::experimental)
FetchContent_MakeAvailable(cppcoro)

#
# Schema library
#
get_filename_component(hw_proto "HelloWorld.proto" ABSOLUTE)
get_filename_component(hw_proto_path "${hw_proto}" PATH)

# Generated sources
set(hw_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/HelloWorld.pb.cc")
set(hw_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/HelloWorld.pb.h")
set(hw_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/HelloWorld.grpc.pb.cc")
set(hw_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/HelloWorld.grpc.pb.h")

add_custom_command(
    OUTPUT "${hw_proto_srcs}" "${hw_proto_hdrs}" "${hw_grpc_srcs}" "${hw_grpc_hdrs}"
    COMMAND ${_PROTOBUF_PROTOC}
    ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
    -I "${hw_proto_path}"
    --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
    "${hw_proto}"
    DEPENDS "${hw_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

add_library(Schema ${hw_proto_srcs} ${hw_grpc_srcs})
target_link_libraries(Schema
    PUBLIC
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF}
)
target_include_directories(Schema PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

#
# Tests Executable
#
add_executable(GrpcPlusCoroutineTest tests.cpp)
target_link_libraries(GrpcPlusCoroutineTest PRIVATE GTest::gmock_main Schema cppcoro)

#
# Main Executable
#
add_executable(GrpcPlusCoroutineBin main.cpp)
target_include_directories(GrpcPlusCoroutineBin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(GrpcPlusCoroutineBin PRIVATE Schema cppcoro)
