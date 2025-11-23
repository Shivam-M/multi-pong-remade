cmake_minimum_required(VERSION 3.10)
project(pong_server)

set(CMAKE_CXX_STANDARD 17)

add_executable(pong_server
    server.cpp
    coordinator.cpp
    protobufs/pong.pb.cc
)

if (WIN32)
    find_package(Protobuf CONFIG REQUIRED)
    target_link_libraries(pong_server PRIVATE protobuf::libprotobuf)
    target_include_directories(pong_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PROTOBUF REQUIRED protobuf)
    target_include_directories(pong_server PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROTOBUF_INCLUDE_DIRS}
    )
    target_link_libraries(pong_server PRIVATE ${PROTOBUF_LIBRARIES})
endif()
