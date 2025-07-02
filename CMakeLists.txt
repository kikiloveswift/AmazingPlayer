cmake_minimum_required(VERSION 3.31)
project(AmazingPlayer)

set(CMAKE_CXX_STANDARD 17)

find_package(glad REQUIRED)
find_package(SDL2 REQUIRED)
find_package(ffmpeg REQUIRED)
get_property(allTargets DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
message(STATUS "ALL CMake targets: ${allTargets}")
foreach(lib avcodec avformat avutil swresample swscale)
    if(TARGET ffmpeg::${lib})
        message(STATUS "ffmpeg::${lib} is FOUND")
    else()
        message(WARNING "ffmpeg::${lib} is NOT found")
    endif()
endforeach()

add_executable(AmazingPlayer src/main.cpp
        src/Render/TriangleRenderer.cpp
        src/Render/TriangleRenderer.h)

target_link_libraries(AmazingPlayer
        PRIVATE
        glad::glad
        SDL2::SDL2
        ffmpeg::avcodec
        ffmpeg::avformat
        ffmpeg::avutil
        ffmpeg::swresample
        ffmpeg::swscale
)
