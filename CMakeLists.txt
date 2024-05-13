cmake_minimum_required(VERSION 3.0)
project (joy-bonnet)
add_executable(joy-bonnet joy-bonnet.c)
install(TARGETS joy-bonnet RUNTIME DESTINATION bin)
