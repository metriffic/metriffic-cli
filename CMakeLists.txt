cmake_minimum_required(VERSION 3.5)

project(metriffic)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug ... FORCE )

find_package(Boost 1.58.0 REQUIRED thread filesystem system chrono)
include_directories(${PROJECT_SOURCE_DIR}/tpt/websocketpp
                    ${PROJECT_SOURCE_DIR}/tpt/cli/include
                    ${PROJECT_SOURCE_DIR}/tpt/json/include
                    ${PROJECT_SOURCE_DIR}/tpt/cxxopts/include
                    ${PROJECT_SOURCE_DIR}/tpt/termcolor/include
                    ${PROJECT_SOURCE_DIR}/tpt/plog/include)

set(sources 
        main.cpp 
        settings_manager.cpp
        ssh_manager
        gql_connection_manager.cpp
        context.cpp
        session_commands.cpp
        authentication_commands.cpp
        query_commands.cpp
        workspace_commands.cpp
        admin_commands.cpp
)
set(libraries 
        ${Boost_LIBRARIES}
        pthread
        crypto
        ssl
        ssh2
        stdc++fs
)


add_executable(metriffic ${sources})
add_executable(metriffic_local ${sources})

target_compile_definitions(metriffic_local
    PRIVATE TEST_MODE
)

set_target_properties(metriffic_local 
    PROPERTIES EXCLUDE_FROM_ALL 1)

target_link_libraries(metriffic ${libraries}) 
target_link_libraries(metriffic_local ${libraries}) 
