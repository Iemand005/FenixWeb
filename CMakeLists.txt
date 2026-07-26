cmake_minimum_required(VERSION 3.15)
project(FenixWeb LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force Emscripten-friendly settings
if(EMSCRIPTEN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sUSE_SDL=2 -sUSE_WEBGL2=1 -sFULL_ES3=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -sUSE_SDL=2 -sUSE_WEBGL2=1 -sFULL_ES3=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']")
endif()

# Exclude native-only subsystems
set(FE_EXCLUDE_GLFW ON CACHE BOOL "Exclude GLFW" FORCE)
set(FE_EXCLUDE_JOLT ON CACHE BOOL "Exclude JOLT Physics" FORCE)
set(FE_INCLUDE_OPENVR OFF CACHE BOOL "Include OpenVR" FORCE)

# Disable WASAPI/GameInput on non-Windows
set(SDL_WASAPI OFF CACHE BOOL "" FORCE)
set(SDL_GAMEINPUT OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)

# Static libs
set(KISSFFT_STATIC ON CACHE BOOL "Build static library" FORCE)
set(GLFW_LIBRARY_TYPE "STATIC" CACHE STRING "" FORCE)

# SDL3 for Emscripten uses SDL2 compat
set(SDL_SHARED DEFAULT CACHE BOOL "" FORCE)
set(SDL_STATIC DEFAULT CACHE BOOL "" FORCE)

set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../FenixEngine")

if(NOT TARGET Engine)
    message(STATUS "Engine target not found. Adding FenixEngine subdirectory...")
    if(EXISTS "${ENGINE_DIR}/CMakeLists.txt")
        add_subdirectory("${ENGINE_DIR}" FenixEngine)
    else()
        message(FATAL_ERROR "FenixEngine not found at ${ENGINE_DIR}!")
    endif()
endif()

# FenixWeb executable - minimal, no ImGui, no Vulkan
add_executable(FenixWeb
    main.cpp
)

target_include_directories(FenixWeb PRIVATE
    ${ENGINE_DIR}
    ${ENGINE_DIR}/include
    ${ENGINE_DIR}/external
    ${ENGINE_DIR}/external/SDL/include
    ${ENGINE_DIR}/src/Engine
)

target_link_libraries(FenixWeb PRIVATE
    Engine
)

# Emscripten output settings
if(EMSCRIPTEN)
    set_target_properties(FenixWeb PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free'] -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
    )
endif()

# Copy resources
add_custom_command(TARGET FenixWeb POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:FenixWeb>/resources
    COMMENT "Copying FenixWeb resources..."
)