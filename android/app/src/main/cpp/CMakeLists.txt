cmake_minimum_required(VERSION 3.22.1)
project(darknes_android_shell C CXX)

# Compile an actual Source SDK mathlib translation unit for Android ARM64.
set(SOURCE_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../../src")
add_library(source_mathlib_android STATIC
    "${SOURCE_SDK_ROOT}/mathlib/vector.cpp"
)
target_include_directories(source_mathlib_android PUBLIC "${SOURCE_SDK_ROOT}/public")
target_compile_definitions(source_mathlib_android PUBLIC POSIX GNUC COMPILER_GCC LINUX _LINUX)
target_compile_options(source_mathlib_android PRIVATE -Wall -Wextra -Wno-unknown-pragmas)
target_compile_features(source_mathlib_android PUBLIC cxx_std_14)

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
    source_mathlib_android
    ${android-lib}
    ${log-lib}
    ${egl-lib}
    ${gles-lib}
    m
)
