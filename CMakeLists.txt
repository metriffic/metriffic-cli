cmake_minimum_required(VERSION 3.5)

project(metriffic)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost 1.58.0 REQUIRED thread filesystem system chrono)
include_directories(${PROJECT_SOURCE_DIR}/tpt/websocketpp
                    ${PROJECT_SOURCE_DIR}/tpt/cli/include
                    ${PROJECT_SOURCE_DIR}/tpt/json/include
                    ${PROJECT_SOURCE_DIR}/tpt/cxxopts/include)

add_executable(metriffic main.cpp 
                         settings_manager.cpp
                         ssh_manager
                         gql_connection_manager.cpp
                         context.cpp
                         session_commands.cpp
                         authentication_commands.cpp
                         query_commands.cpp)
target_link_libraries(metriffic ${Boost_LIBRARIES}
                                pthread
                                crypto
                                ssl
                                ssh2
                                stdc++fs) 
