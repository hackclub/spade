cmake_minimum_required(VERSION 3.24)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (NOT SPADE_TARGET)
  set(SPADE_TARGET pc)
endif()

# would be nice if this could be in rp2/CMakeLists.txt,
# but has to be before project() and project() has to be top level
if (SPADE_TARGET STREQUAL "rpi")
  include(rpi/pico_sdk_import.cmake)
  include(rpi/pico_extras_import.cmake)
endif()

project(spade C CXX)
add_executable(spade "${SPADE_TARGET}/main.c")
include("${SPADE_TARGET}/CMakeLists.txt")

add_library(JerryXX jerryxx.c)
target_link_libraries(spade
  JerryXX
  "${CMAKE_CURRENT_SOURCE_DIR}/${SPADE_TARGET}/jerry/lib/libjerry-core.a"
  "${CMAKE_CURRENT_SOURCE_DIR}/${SPADE_TARGET}/jerry/lib/libjerry-ext.a"
  "${CMAKE_CURRENT_SOURCE_DIR}/${SPADE_TARGET}/jerry/lib/libjerry-port-default.a"
)
include_directories(${SPADE_TARGET}/jerry/include)
include_directories(./)
