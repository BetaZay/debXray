cmake_minimum_required(VERSION 3.10)
project(debXray)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

# Include SDL2 and SDL2_ttf
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(OpenCV REQUIRED)

# ImGui sources
set(IMGUI_DIR imgui)
set(IMGUI_BACKENDS ${IMGUI_DIR}/backends)

set(IMGUI_SRC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_BACKENDS}/imgui_impl_sdl2.cpp
    ${IMGUI_BACKENDS}/imgui_impl_sdlrenderer2.cpp
)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${IMGUI_DIR}
    ${IMGUI_BACKENDS}
    include
)

add_executable(debXray
    ${IMGUI_SRC}
    src/main.cpp
    src/SystemInfo.cpp
    src/DependencyManager.cpp
    src/Renderer.cpp
    src/Log.cpp
    src/WebcamFeed.cpp 
)

add_custom_command(TARGET debXray POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:debXray>/libs"
    COMMAND ldd $<TARGET_FILE:debXray> | awk '/=> \// {print $3}' |
            xargs -I '{}' cp '{}' "$<TARGET_FILE_DIR:debXray>/libs"
    COMMENT "Bundling shared libraries..."
)

target_link_libraries(debXray
    ${OpenCV_LIBS}
    ${SDL2_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
)
