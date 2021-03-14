cmake_minimum_required(VERSION 3.14)

project(igal LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_definitions(/W0)
else()
    add_definitions(-w)
endif()

add_subdirectory(igal)