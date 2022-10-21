cmake_minimum_required(VERSION 3.24)
set(CMAKE_C_STANDARD 99)

project(spade C)
add_executable(spade main.c)

add_library(JerryXX jerryxx.c)

target_link_libraries(spade
  m
  JerryXX 
  "${CMAKE_CURRENT_SOURCE_DIR}/jerry/lib/libjerry-core.a"
  "${CMAKE_CURRENT_SOURCE_DIR}/jerry/lib/libjerry-ext.a"
  "${CMAKE_CURRENT_SOURCE_DIR}/jerry/lib/libjerry-port-default.a"
)
include_directories(jerry/include)
include_directories(/usr/include/libxkbcommon)

add_subdirectory(deps/minifb)
target_link_libraries(spade minifb)
