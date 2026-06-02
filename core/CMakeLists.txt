cmake_minimum_required(VERSION 3.16)

if(POLICY CMP0141)
    cmake_policy(SET CMP0141 NEW)
endif()
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

project(hftext_core LANGUAGES CXX)

add_library(hftext_core STATIC
    src/crc16.cpp
    src/demodulator_fsk.cpp
    src/encoder.cpp
    src/frame.cpp
    src/hftext_core.cpp
    src/modulator_fsk.cpp
)

target_compile_features(hftext_core PUBLIC cxx_std_17)

target_include_directories(hftext_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    set_target_properties(hftext_core PROPERTIES
        MSVC_DEBUG_INFORMATION_FORMAT Embedded
    )
endif()

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(HFTEXT_CORE_BUILD_TESTS_DEFAULT ON)
    enable_testing()
else()
    set(HFTEXT_CORE_BUILD_TESTS_DEFAULT OFF)
endif()

option(HFTEXT_CORE_BUILD_TESTS "Build HFText core tests" ${HFTEXT_CORE_BUILD_TESTS_DEFAULT})

if(HFTEXT_CORE_BUILD_TESTS)
    add_executable(hftext_core_types_test
        tests/test_core_types.cpp
    )
    add_executable(hftext_core_encoder_test
        tests/test_encoder.cpp
    )
    add_executable(hftext_core_crc16_test
        tests/test_crc16.cpp
    )
    add_executable(hftext_core_frame_test
        tests/test_frame.cpp
    )
    add_executable(hftext_core_modulator_test
        tests/test_modulator.cpp
    )
    add_executable(hftext_core_demodulator_test
        tests/test_demodulator.cpp
    )

    target_link_libraries(hftext_core_types_test PRIVATE hftext_core)
    target_link_libraries(hftext_core_encoder_test PRIVATE hftext_core)
    target_link_libraries(hftext_core_crc16_test PRIVATE hftext_core)
    target_link_libraries(hftext_core_frame_test PRIVATE hftext_core)
    target_link_libraries(hftext_core_modulator_test PRIVATE hftext_core)
    target_link_libraries(hftext_core_demodulator_test PRIVATE hftext_core)

    if(MSVC)
        set_target_properties(
            hftext_core_types_test
            hftext_core_encoder_test
            hftext_core_crc16_test
            hftext_core_frame_test
            hftext_core_modulator_test
            hftext_core_demodulator_test
            PROPERTIES
                MSVC_DEBUG_INFORMATION_FORMAT Embedded
        )
    endif()

    add_test(NAME hftext_core_types_test COMMAND hftext_core_types_test)
    add_test(NAME hftext_core_encoder_test COMMAND hftext_core_encoder_test)
    add_test(NAME hftext_core_crc16_test COMMAND hftext_core_crc16_test)
    add_test(NAME hftext_core_frame_test COMMAND hftext_core_frame_test)
    add_test(NAME hftext_core_modulator_test COMMAND hftext_core_modulator_test)
    add_test(NAME hftext_core_demodulator_test COMMAND hftext_core_demodulator_test)
endif()
