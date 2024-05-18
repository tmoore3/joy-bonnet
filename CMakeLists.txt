cmake_minimum_required(VERSION 3.0)
project (joy-bonnet)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBEVDEV REQUIRED libevdev)

add_executable(joy-bonnet joy-bonnet.cpp)
install(TARGETS joy-bonnet RUNTIME DESTINATION bin)

target_include_directories(joy-bonnet PRIVATE ${LIBEVDEV_INCLUDE_DIRS})
target_link_libraries(joy-bonnet PRIVATE ${LIBEVDEV_LIBRARIES})
