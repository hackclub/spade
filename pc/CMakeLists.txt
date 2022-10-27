target_link_libraries(spade -fno-omit-frame-pointer -fsanitize=undefined)

add_subdirectory(pc/deps/minifb)
target_link_libraries(spade minifb)
