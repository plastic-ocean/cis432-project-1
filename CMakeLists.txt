cmake_minimum_required(VERSION 3.3)
project(cis432_project_1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    client.c
    client.h
    duckchat.h
    main.cpp
    raw.c
    raw.h
    server.c
    server.h)

add_executable(cis432_project_1 ${SOURCE_FILES})