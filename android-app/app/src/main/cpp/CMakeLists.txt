cmake_minimum_required(VERSION 3.22.1)

project(hftext_android LANGUAGES C CXX)

set(HFTEXT_CORE_BUILD_TESTS OFF CACHE BOOL "Build HFText core tests" FORCE)
set(HFTEXT_CORE_BUILD_TOOLS OFF CACHE BOOL "Build HFText CLI tools" FORCE)

add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../core"
    "${CMAKE_CURRENT_BINARY_DIR}/hftext_core"
)

add_library(hftext_android_jni SHARED
    hftext_android_jni.cpp
)

target_compile_features(hftext_android_jni PRIVATE cxx_std_17)

target_include_directories(hftext_android_jni
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/../../../../../core/include"
)

target_link_libraries(hftext_android_jni
    PRIVATE
        hftext_c_api_shared
)
