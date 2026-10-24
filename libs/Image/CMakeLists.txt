add_library(Image
        Image.h
        ImageDisplay.cpp ImageDisplay.h
        AnimatedVideo.cpp AnimatedVideo.h
)

target_include_directories(Image PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/external/stb
)

find_package(Threads REQUIRED)

# FFmpeg decodes video animations (MSYS2: mingw-w64-x86_64-ffmpeg; Debian/Pi: libavformat-dev
# libavcodec-dev libswscale-dev)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libswscale libavutil)

target_link_libraries(Image PUBLIC
        ImGui
        OpenGL::GL
        glfw
        GLEW::GLEW
        Threads::Threads
        PkgConfig::FFMPEG
)
