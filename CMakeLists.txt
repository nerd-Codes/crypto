cmake_minimum_required(VERSION 3.15)
project(GoQuantEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This says: create an executable named "engine" from main.cpp
# CMake is smart enough to see that main.cpp includes Order.h,
# so it will automatically re-compile if you change Order.h.
add_executable(engine
    main.cpp
    Order.cpp
    Trade.cpp )