cmake_minimum_required(VERSION 3.20)
project(focus_zones C)

set(CMAKE_C_STANDARD 11)

add_executable(focus_zones WIN32
    src/main.c
    src/config.c
    src/layout.c
    src/resize.c
    src/swap.c
    src/window.c
    src/tray.c
    src/overlay.c
    src/hotkey.c
    vendor/cJSON/cJSON.c
)

target_include_directories(focus_zones PRIVATE
    src
    vendor/cJSON
)

target_link_libraries(focus_zones PRIVATE
    user32
    dwmapi
    kernel32
    shell32
    shcore
    gdi32
)

target_compile_definitions(focus_zones PRIVATE
    _UNICODE
    UNICODE
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
)
