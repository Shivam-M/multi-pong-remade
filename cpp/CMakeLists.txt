cmake_minimum_required(VERSION 3.10)
project(multi_pong)

set(CMAKE_CXX_STANDARD 17)

add_executable(multi_pong
    external/glad.c
    tools/renderer_opengl.cpp
    client.cpp
    server.cpp
    coordinator.cpp
    protobufs/pong.pb.cc)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

target_include_directories(multi_pong PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(multi_pong PRIVATE glfw)

if (WIN32)
    find_package(Protobuf CONFIG REQUIRED)
    target_link_libraries(multi_pong PRIVATE protobuf::libprotobuf)
    target_include_directories(multi_pong PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PROTOBUF REQUIRED protobuf)
    target_include_directories(multi_pong PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROTOBUF_INCLUDE_DIRS}
    )
    target_link_libraries(multi_pong PRIVATE ${PROTOBUF_LIBRARIES})
endif()
