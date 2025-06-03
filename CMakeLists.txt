cmake_minimum_required(VERSION 3.18)
project(debXray LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

# Use static linking for SDL and TTF
set(SDL2_USE_STATIC_LIBS ON)
set(SDL2TTF_USE_STATIC_LIBS ON)
set(OpenCV_STATIC OFF)  # Only OpenCV is allowed dynamically

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

# Dependencies
find_package(SDL2 REQUIRED CONFIG)
find_package(SDL2_ttf REQUIRED CONFIG)
find_package(OpenCV REQUIRED)

# Main binary
add_executable(debXray
    ${IMGUI_SRC}
    src/main.cpp
    src/SystemInfo.cpp
    src/DependencyManager.cpp
    src/Renderer.cpp
    src/Log.cpp
    src/WebcamFeed.cpp
)

target_include_directories(debXray PRIVATE
    ${IMGUI_DIR}
    ${IMGUI_BACKENDS}
    include
)

# Static linking of C++ stdlib and system libs
target_link_options(debXray PRIVATE
    -static-libgcc
    -static-libstdc++
    -Wl,--copy-dt-needed-entries
)

# Link static where we can, dynamic for OpenCV
target_link_libraries(debXray PRIVATE
    -Wl,-Bstatic
    SDL2::SDL2-static
    SDL2_ttf::SDL2_ttf-static
    -Wl,-Bdynamic
    ${OpenCV_LIBS}
    m pthread dl
)

# Output directory
set_target_properties(debXray PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Packaging
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_NAME "debXray")
set(CPACK_PACKAGE_VENDOR "Isaiah Clark")
set(CPACK_PACKAGE_CONTACT "support@yourdomain.com")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Isaiah Clark")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

install(TARGETS debXray RUNTIME DESTINATION bin)
include(CPack)
