cmake_minimum_required(VERSION 3.15)
project(FenixWeb LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Engine configuration - must be set before add_subdirectory
# ============================================================
set(FE_EXCLUDE_GLFW ON CACHE BOOL "Exclude GLFW from the build" FORCE)
set(FE_EXCLUDE_JOLT ON CACHE BOOL "Exclude JOLT physics from the build" FORCE)
set(FE_EXCLUDE_VULKAN ON CACHE BOOL "Exclude Vulkan from the build" FORCE)
set(FE_EXCLUDE_OPENXR ON CACHE BOOL "Exclude OpenXR from the build" FORCE)
set(FE_INCLUDE_OPENVR OFF CACHE BOOL "Include OpenVR" FORCE)

set(SDL_WASAPI OFF CACHE BOOL "" FORCE)
set(SDL_GAMEINPUT OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK ON CACHE BOOL "" FORCE)

# ============================================================
# Engine path
# ============================================================
set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../FenixEngine")

if(NOT TARGET Engine)
    message(STATUS "Engine target not found. Adding FenixEngine from ${ENGINE_DIR}")
    if(EXISTS "${ENGINE_DIR}/CMakeLists.txt")
        add_subdirectory("${ENGINE_DIR}" FenixEngine)
    else()
        message(FATAL_ERROR "FenixEngine not found at ${ENGINE_DIR}!")
    endif()
endif()

# ============================================================
# ImGui (optional - for web debugging UI)
# ============================================================
set(IMGUI_DIR ../FenixEngine/external/imgui)

file(GLOB_RECURSE IMGUI_SOURCE_FILES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
)

# ============================================================
# FenixWeb executable
# ============================================================
if(EMSCRIPTEN)
    add_executable(FenixWeb
        ${IMGUI_SOURCE_FILES}
        main.cpp
    )
else()
    add_executable(FenixWeb WIN32
        ${IMGUI_SOURCE_FILES}
        main.cpp
    )
endif()

target_include_directories(FenixWeb PRIVATE
    ${ENGINE_DIR}
    ${ENGINE_DIR}/include
    ${ENGINE_DIR}/external
    ${ENGINE_DIR}/external/imgui
    ${ENGINE_DIR}/external/SDL/include
    ${ENGINE_DIR}/src/Engine
)

target_link_libraries(FenixWeb PRIVATE
    Engine
)

if(MSVC)
    target_compile_options(FenixWeb PRIVATE $<$<CONFIG:Debug>:/Zi>)
    target_link_options(FenixWeb PRIVATE $<$<CONFIG:Debug>:/DEBUG>)
endif()

# Copy resources
add_custom_command(TARGET FenixWeb POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:FenixWeb>/resources
    COMMENT "Copying FenixWeb resources..."
)

# EMScripten-specific: all -s flags go in linker flags only
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    set(EMSCRIPTEN_LINK_FLAGS
        "-s USE_SDL=3"
        "-s USE_WEBGL2=1"
        "-s FULL_ES3=1"
        "-s GL_ENABLE_GET_PROC_ADDRESS=1"
        "-s ALLOW_MEMORY_GROWTH=1"
        "-s INITIAL_MEMORY=64MB"
        "-s NO_EXIT_RUNTIME=1"
        "-s EXIT_RUNTIME=0"
        "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-s EXPORTED_FUNCTIONS=['_main']"
        "-s MODULARIZE=1"
        "-s EXPORT_NAME=FenixWeb"
        "-s MAX_WEBGL_VERSION=2"
        "-s MIN_WEBGL_VERSION=2"
        "-Os"
        "-flto"
    )
    string(REPLACE ";" " " EMSCRIPTEN_LINK_FLAGS_STR "${EMSCRIPTEN_LINK_FLAGS}")

    set_target_properties(FenixWeb PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS_STR}"
    )
endif()
