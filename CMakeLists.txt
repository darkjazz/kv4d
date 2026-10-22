cmake_minimum_required(VERSION 3.16)
project(kv4d CXX)

# Cinder 0.9.2 built as a static library (lib/macosx/<Config>/libcinder.a).
set(CINDER_PATH "$ENV{HOME}/libdev/cinder_0.9.2" CACHE PATH "Cinder root")
if(NOT EXISTS "${CINDER_PATH}/include/cinder/Cinder.h")
    message(FATAL_ERROR "Cinder not found at ${CINDER_PATH}; pass -DCINDER_PATH=...")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()
# libcinder.a is x86_64 only
set(CMAKE_OSX_ARCHITECTURES x86_64)
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)

# ---- pure C++ core (no Cinder): CPU reference + rule search tool ----
add_library(kv4d_core STATIC src/sim/Rule4D.cpp src/sim/Grid4D.cpp)
target_include_directories(kv4d_core PUBLIC src)

add_executable(rulesearch tools/rulesearch.cpp)
target_link_libraries(rulesearch kv4d_core)

# ---- app ----
set(SOURCES
    src/kv4dApp.cpp
    src/Session.cpp
    src/sim/World4D.cpp
    src/view/Renderer4D.cpp
    src/view/Efx.cpp
    src/io/Audio.cpp
    src/io/Osc4D.cpp
    ${CINDER_PATH}/blocks/OSC/src/cinder/osc/Osc.cpp
)

add_executable(kv4d MACOSX_BUNDLE ${SOURCES})
# Cinder 0.9.2's bundled boost headers need auto_ptr, which C++17 removed.
set_target_properties(kv4d PROPERTIES CXX_STANDARD 14)
target_include_directories(kv4d PRIVATE
    src
    ${CINDER_PATH}/include
    ${CINDER_PATH}/blocks/OSC/src
)
target_compile_options(kv4d PRIVATE -Wno-deprecated-declarations -Wno-comma)
target_link_libraries(kv4d PRIVATE
    kv4d_core
    "${CINDER_PATH}/lib/macosx/$<IF:$<CONFIG:Debug>,Debug,Release>/libcinder.a"
    "-framework Cocoa" "-framework OpenGL" "-framework AppKit" "-framework Foundation"
    "-framework CoreVideo" "-framework CoreMedia" "-framework AVFoundation"
    "-framework Accelerate" "-framework AudioToolbox" "-framework AudioUnit"
    "-framework CoreAudio" "-framework IOKit" "-framework IOSurface"
)
set_target_properties(kv4d PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resources/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.kurivari.kv4d"
    MACOSX_BUNDLE_BUNDLE_NAME "kv4d"
)

# Cinder searches for an "assets" folder starting at the executable and walking
# up a few parents, so a copy next to the binary inside the bundle is found.
add_custom_command(TARGET kv4d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:kv4d>/assets"
    COMMENT "Copying assets into kv4d.app")
