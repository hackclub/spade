target_link_libraries(spade -fno-omit-frame-pointer -fsanitize=undefined)

add_library(Audio pc/audio.c)
find_library(COREAUDIO_LIBRARY NAMES CoreAudio)
find_library(AUDIOUNIT_LIBRARY NAMES AudioUnit)
target_link_libraries(Audio PRIVATE ${COREAUDIO_LIBRARY} ${AUDIOUNIT_LIBRARY})

add_subdirectory(pc/deps/minifb)
target_link_libraries(spade Audio minifb)
