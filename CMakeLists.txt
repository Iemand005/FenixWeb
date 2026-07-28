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

# ============================================================
# Emscripten build flags
# ============================================================
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")

    # Debug: full symbols + source maps
    target_compile_options(FenixWeb PRIVATE
        $<$<CONFIG:Debug>:-g -fdebug-compilation-dir="${CMAKE_SOURCE_DIR}">
    )
    target_link_options(FenixWeb PRIVATE
        $<$<CONFIG:Debug>:-g -gsource-map>
    )

    # Release: maximum size optimization, no debug info
    target_compile_options(FenixWeb PRIVATE
        $<$<CONFIG:Release>:-Oz -flto>
    )
    target_link_options(FenixWeb PRIVATE
        $<$<CONFIG:Release>:-Oz -flto --closure 1 -sASSERTIONS=0>
    )

    file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources" _res_cmake_path)

    set_target_properties(FenixWeb PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "-sUSE_WEBGL2=1 -sFULL_ES3=1 -sGL_ENABLE_GET_PROC_ADDRESS=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=64MB -sNO_EXIT_RUNTIME=1 -sEXIT_RUNTIME=0 -sMAX_WEBGL_VERSION=2 -sMIN_WEBGL_VERSION=2 -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=FS,ccall,cwrap -sEXPORTED_FUNCTIONS=_main,_emscripten_file_dialog_callback,_malloc,_free --preload-file ${_res_cmake_path}@resources"
    )

    # Deploy binaries to FenixWeb root for serving
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(DEPLOY_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        add_custom_command(TARGET FenixWeb POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE_DIR:FenixWeb>/FenixWeb.html" "${DEPLOY_DIR}/FenixWeb.html"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE_DIR:FenixWeb>/FenixWeb.js" "${DEPLOY_DIR}/FenixWeb.js"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE_DIR:FenixWeb>/FenixWeb.wasm" "${DEPLOY_DIR}/FenixWeb.wasm"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE_DIR:FenixWeb>/FenixWeb.data" "${DEPLOY_DIR}/FenixWeb.data"
            COMMENT "Deploying FenixWeb binaries to ${DEPLOY_DIR}..."
        )
    endif()
endif()
