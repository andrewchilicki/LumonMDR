add_library(Image
        Image.h
        ImageDisplay.cpp ImageDisplay.h
)

target_include_directories(Image PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/external/stb
)

target_link_libraries(Image PUBLIC
        ImGui
        OpenGL::GL
        glfw
        GLEW::GLEW
)