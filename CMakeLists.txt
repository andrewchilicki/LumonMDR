cmake_minimum_required(VERSION 3.10)
project(LumonMDR)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CROSSCOMPILING)
    # Compile for Raspberry Pi
    message(STATUS "Compiling for Raspberry Pi...")

    # Setup paths
    if(NOT DEFINED CROSS_COMPILE_SYSROOT)
        message(FATAL_ERROR "Please set -DCROSS_COMPILE_SYSROOT=<path> to specify sysroot.")
    endif()

    set(CMAKE_SYSROOT ${CROSS_COMPILE_SYSROOT})
    set(RPI_LIB_PATH "${CMAKE_SYSROOT}/usr/lib/arm-linux-gnueabihf")
    set(RPI_INCLUDE_PATH "${CMAKE_SYSROOT}/usr/include")

    list(APPEND CMAKE_LIBRARY_PATH "${RPI_LIB_PATH}")
    list(APPEND CMAKE_INCLUDE_PATH "${RPI_INCLUDE_PATH}")
    list(APPEND CMAKE_PREFIX_PATH "${RPI_LIB_PATH}/cmake")

    # Use pkg-config to find OpenGL
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(OpenGL REQUIRED gl)

    # GLFW paths
    set(glfw3_DIR "${RPI_LIB_PATH}/cmake/glfw3/")
    list(APPEND CMAKE_PREFIX_PATH "${glfw3_DIR}")
endif()

# OpenGL
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

# GLFW
find_package(glfw3 REQUIRED)

# ImGui
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)

add_library(ImGui
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(ImGui PUBLIC
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
)

target_link_libraries(ImGui PUBLIC glfw OpenGL::GL)

# libdl
find_library(DL_LIBRARY NAMES dl)
if(NOT DL_LIBRARY)
    message(FATAL_ERROR "libdl not found.")
endif()

# Nlohmann
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_SOURCE_DIR}/external/nlohmann)

# Add libraries subdirectory
add_subdirectory(libs)

# Add main executable
add_executable(${PROJECT_NAME} src/main.cpp
        src/UI/UIManager.cpp
        src/UI/UIManager.h
        src/UI/Widgets/NumbersPanel.cpp
        src/UI/Widgets/NumbersPanel.h
        src/UI/Widgets/IdleScreen.cpp
        src/UI/Widgets/IdleScreen.h
        src/UI/Widgets/BootSequence.h
        src/UI/Widgets/BootSequence.cpp
        src/UI/Widgets/Settings.h)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
        ${CMAKE_SOURCE_DIR}/external/perlin-noise
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        glfw
        OpenGL::GL
        ImGui
        Image
        Numbers
        ${DL_LIBRARY}
        X11
        Xrandr
        Xinerama
        Xxf86vm
        Xcursor
        nlohmann_json
)
