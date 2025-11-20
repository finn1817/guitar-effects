cmake_minimum_required(VERSION 3.16)
project(GuitarEffectsApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

# Include miniaudio
include_directories(${CMAKE_SOURCE_DIR}/miniaudio)

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/AudioEngine.cpp
    src/DSPChain.cpp
    src/Looper.cpp
    src/Recorder.cpp
    src/ClipManager.cpp
    src/PitchShifter.cpp
)

set(HEADERS
    src/MainWindow.h
    src/AudioEngine.h
    src/DSPChain.h
    src/Looper.h
    src/Recorder.h
    src/ClipManager.h
    src/PitchShifter.h
)

# Create executable
if(WIN32)
    add_executable(GuitarEffectsApp WIN32 ${SOURCES} ${HEADERS})
elseif(APPLE)
    add_executable(GuitarEffectsApp MACOSX_BUNDLE ${SOURCES} ${HEADERS})
else()
    add_executable(GuitarEffectsApp ${SOURCES} ${HEADERS})
endif()

# Link Qt libraries
target_link_libraries(GuitarEffectsApp
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

# Platform-specific linking
if(WIN32)
    target_link_libraries(GuitarEffectsApp winmm ole32)
elseif(APPLE)
    target_link_libraries(GuitarEffectsApp "-framework CoreAudio" "-framework AudioToolbox")
else()
    target_link_libraries(GuitarEffectsApp asound pthread m)
endif()

# Installation
install(TARGETS GuitarEffectsApp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)