cmake_minimum_required(VERSION 3.16)
project(joseph)

set(CMAKE_CXX_STANDARD 20)

add_executable(joseph
        examples/main.cpp
        src/Joseph/Server.cpp
        src/Joseph/Server.h
        src/Joseph/Request.cpp
        src/Joseph/Request.h
        src/Joseph/Response.cpp
        src/Joseph/Response.h
        src/Joseph/Route.h
        src/Joseph/KVList.h
        src/Joseph/Helpers.h
        external/minjson.hpp
        src/Joseph/ThreadPool.h
        src/Joseph/Config.h
        src/Joseph/MultipartParser.cpp
        src/Joseph/MultipartParser.h
        src/Joseph/Ratelimit.h
)
target_include_directories(joseph PRIVATE external src)

if(WIN32)
    target_link_libraries(joseph Ws2_32)
endif()
