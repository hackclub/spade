target_link_libraries(spade -fno-omit-frame-pointer -fsanitize=undefined)

if (APPLE)
  add_definitions(-DSPADE_AUDIO)

  add_library(Audio pc/audio.c)
  find_library(COREAUDIO_LIBRARY NAMES CoreAudio)
  find_library(AUDIOUNIT_LIBRARY NAMES AudioUnit)
  target_link_libraries(Audio PRIVATE ${COREAUDIO_LIBRARY} ${AUDIOUNIT_LIBRARY})

  add_subdirectory(pc/deps/minifb)
  target_link_libraries(spade Audio minifb)
else()
  message(WARNING "Spade audio is only supported on macOS, audio will be muted")
  add_subdirectory(pc/deps/minifb)
  target_link_libraries(spade minifb)
endif()

set(CMAKE_CXX_CLANG_TIDY
  clang-tidy-11;
  -format-style='file';
  -header-filter=${CMAKE_CURRENT_SOURCE_DIR};
)
