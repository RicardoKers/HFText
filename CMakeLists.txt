cmake_minimum_required(VERSION 3.16)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

project(hftext LANGUAGES CXX)

enable_testing()

option(HFTEXT_BUILD_CORE_TESTS "Build HFText core tests from the root project" ON)
set(HFTEXT_CORE_BUILD_TESTS ${HFTEXT_BUILD_CORE_TESTS} CACHE BOOL "Build HFText core tests" FORCE)

add_subdirectory(core)
add_subdirectory(pc-app)
