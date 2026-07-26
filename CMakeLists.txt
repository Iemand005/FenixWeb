cmake_minimum_required(VERSION 3.15)
project(FenixWeb LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    Renderer
)

if(EMSCRIPTEN)
    target_compile_definitions(FenixWeb PRIVATE
        FE_EXCLUDE_GLFW
        FE_EXCLUDE_VULKAN
        FE_EXCLUDE_JOLT
    )
endif()

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

# EMScripten-specific: linker flags only (NO -s USE_SDL=3, Engine builds SDL from source)
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
    set_target_properties(FenixWeb PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "-s USE_WEBGL2=1 -s FULL_ES3=1 -sGL_ENABLE_GET_PROC_ADDRESS=1 -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=64MB -s NO_EXIT_RUNTIME=1 -s EXIT_RUNTIME=0 -s MAX_WEBGL_VERSION=2 -s MIN_WEBGL_VERSION=2 -Os -flto"
    )
endif()
