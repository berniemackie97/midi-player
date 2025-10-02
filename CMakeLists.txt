# CMakeLists.txt (project root)
cmake_minimum_required(VERSION 3.16)
project(midi_player LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sources
add_executable(midi_player
  src/main.cpp
  src/midi/tempo.cpp
  src/midi/smf.cpp
  src/assets/sf_resolver.cpp
  src/audio/player.cpp        # ← NEW: audio engine
)

# Headers live under src/ and thirdparty/
target_include_directories(midi_player PRIVATE
  src
  thirdparty
)

# Warnings
if(MSVC)
  target_compile_options(midi_player PRIVATE /W4 /permissive-)
else()
  target_compile_options(midi_player PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Threads (miniaudio uses std::thread in our player)
find_package(Threads REQUIRED)
target_link_libraries(midi_player PRIVATE Threads::Threads)

# macOS audio frameworks for miniaudio’s CoreAudio backend
if(APPLE)
  target_link_libraries(midi_player PRIVATE
    "-framework AudioToolbox"
    "-framework AudioUnit"
    "-framework CoreAudio"
    "-framework CoreFoundation"
  )
endif()

