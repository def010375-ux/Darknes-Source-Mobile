cmake_minimum_required(VERSION 3.22.1)
project(darknes_android_shell C)

set(NATIVE_APP_GLUE_DIR "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")

add_library(darknes_android_shell SHARED
    main.c
    "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c"
)

target_include_directories(darknes_android_shell PRIVATE "${NATIVE_APP_GLUE_DIR}")
target_compile_options(darknes_android_shell PRIVATE
    -Wall
    -Wextra
    -Werror=return-type
)

find_library(android-lib android)
find_library(log-lib log)
find_library(egl-lib EGL)
find_library(gles-lib GLESv2)

target_link_libraries(darknes_android_shell
    ${android-lib}
    ${log-lib}
    ${egl-lib}
    ${gles-lib}
)
