cmake_minimum_required (VERSION 2.6)
project (mrtgen)

#debug symbols
set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE RelWithDebInfo)
#set(CMAKE_BUILD_TYPE Release)

set(PROJECT_SOURCE_DIR ./)
set(PROJECT_HEADER_DIR ./)
set(HEADER_INSTALL_DIR /usr/local/include/)

add_executable(mrtgen mrtgen.c)
#target_compile_options(fasthash_test PRIVATE -Wall -Wextra -pedantic -Werror)
