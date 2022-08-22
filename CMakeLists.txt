cmake_minimum_required(VERSION 3.5)

project(metriffic)

find_library(SSH2_LIBRARY NAMES ssh2 libssh2)

set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE INTERNAL "" FORCE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug ... FORCE )
set(OPENSSL_ROOT_DIR /usr/local/opt/openssl@3)
set(SSH2_ROOT_DIR} /usr/local/opt/libssh2)

add_compile_definitions(BOOST_ASIO_USE_TS_EXECUTOR_AS_DEFAULT=1)

find_package(Boost 1.58.0 REQUIRED thread filesystem system chrono)


include_directories(${OPENSSL_ROOT_DIR}/include
					${SSH2_ROOT_DIR}/include
					${PROJECT_SOURCE_DIR}/tpt/websocketpp
                    ${PROJECT_SOURCE_DIR}/tpt/cli/include
                    ${PROJECT_SOURCE_DIR}/tpt/json/include
                    ${PROJECT_SOURCE_DIR}/tpt/cxxopts/include
                    ${PROJECT_SOURCE_DIR}/tpt/termcolor/include
                    ${PROJECT_SOURCE_DIR}/tpt/plog/include)

link_directories(${OPENSSL_ROOT_DIR}/lib)

set(sources 
        main.cpp 
        settings_manager.cpp
        ssh_manager.cpp
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
		${SSH2_LIBRARY}
		ssl
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
