cmake_minimum_required(VERSION 3.11)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -fPIC")

set(ENV{PACKAGE_VERSION} "4.0")

# set(CMAKE_BUILD_TYPE Debug)

project(marss)

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET glib-2.0 libxml-2.0 libcurl)

add_executable(marss main.cpp FeedsFile.cpp XmlParser.cpp)
target_link_libraries(marss PkgConfig::deps)

install(TARGETS marss DESTINATION bin)