target_link_libraries(spade -fno-omit-frame-pointer -fsanitize=undefined)

add_subdirectory(deps/minifb)
target_link_libraries(spade minifb)
