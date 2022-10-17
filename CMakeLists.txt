cmake_minimum_required(VERSION 3.24)
set(CMAKE_C_STANDARD 99)

project(spade C)
add_executable(spade main.c)
target_link_libraries(spade PRIVATE
  jerry/lib/libjerry-core.a
  jerry/lib/libjerry-ext.a
  jerry/lib/libjerry-port-default.a
)
include_directories(jerry/include)

add_subdirectory(deps/minifb)
target_link_libraries(spade minifb)
