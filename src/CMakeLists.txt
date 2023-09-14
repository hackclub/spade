add_executable(spade "${SPADE_TARGET}/main.c")
include("${SPADE_TARGET}/CMakeLists.txt")

add_library(JerryXX shared/js_runtime/jerryxx.c)
add_library(Piano shared/audio/piano.c)

target_link_libraries(spade
  Piano
  JerryXX
  "${CMAKE_CURRENT_SOURCE_DIR}/${SPADE_TARGET}/jerry/lib/libjerry-core.a"
  "${CMAKE_CURRENT_SOURCE_DIR}/${SPADE_TARGET}/jerry/lib/libjerry-ext.a"
  "${CMAKE_CURRENT_SOURCE_DIR}/${SPADE_TARGET}/jerry/lib/libjerry-port-default.a"
)
include_directories(${SPADE_TARGET}/jerry/include)
include_directories(./)
