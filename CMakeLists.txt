cmake_minimum_required(VERSION 3.5)

project(Metriffic)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Boost 1.58.0 REQUIRED thread filesystem system chrono)

include_directories(${PROJECT_SOURCE_DIR}/tpt/websocketpp)

add_executable(Metriffic metriffic.cpp)
target_link_libraries(Metriffic ${Boost_LIBRARIES}
                                pthread
                                crypto
                                ssl) 
