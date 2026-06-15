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
    src/robust.cpp
    src/streaming_receiver.cpp
)

add_library(hftext_wav_io STATIC
    tools/wav_io.cpp
)

target_compile_features(hftext_core PUBLIC cxx_std_17)

target_include_directories(hftext_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(hftext_wav_io
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/tools
)

target_compile_features(hftext_wav_io PUBLIC cxx_std_17)

add_executable(hftext_tx_wav
    tools/hftext_tx_wav.cpp
)
add_executable(hftext_rx_wav
    tools/hftext_rx_wav.cpp
)
add_executable(hftext_stream_wav
    tools/hftext_stream_wav.cpp
)

target_link_libraries(hftext_tx_wav PRIVATE hftext_core hftext_wav_io)
target_link_libraries(hftext_rx_wav PRIVATE hftext_core hftext_wav_io)
target_link_libraries(hftext_stream_wav PRIVATE hftext_core hftext_wav_io)

if(MSVC)
    set_target_properties(hftext_core PROPERTIES
        MSVC_DEBUG_INFORMATION_FORMAT Embedded
    )
    set_target_properties(hftext_wav_io hftext_tx_wav hftext_rx_wav hftext_stream_wav PROPERTIES
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
    add_executable(hftext_core_api_test
        tests/test_core_api.cpp
    )
    add_executable(hftext_wav_io_test
        tests/test_wav_io.cpp
    )
    add_executable(hftext_streaming_receiver_test
        tests/test_streaming_receiver.cpp
    )
    add_executable(hftext_robust_test
        tests/test_robust.cpp
    )

    target_link_libraries(hftext_core_types_test PRIVATE hftext_core)
    target_link_libraries(hftext_core_encoder_test PRIVATE hftext_core)
    target_link_libraries(hftext_core_crc16_test PRIVATE hftext_core)
    target_link_libraries(hftext_core_frame_test PRIVATE hftext_core)
    target_link_libraries(hftext_core_modulator_test PRIVATE hftext_core)
    target_link_libraries(hftext_core_demodulator_test PRIVATE hftext_core)
    target_link_libraries(hftext_core_api_test PRIVATE hftext_core)
    target_link_libraries(hftext_wav_io_test PRIVATE hftext_wav_io)
    target_link_libraries(hftext_streaming_receiver_test PRIVATE hftext_core)
    target_link_libraries(hftext_robust_test PRIVATE hftext_core)

    if(MSVC)
        set_target_properties(
            hftext_core_types_test
            hftext_core_encoder_test
            hftext_core_crc16_test
            hftext_core_frame_test
            hftext_core_modulator_test
            hftext_core_demodulator_test
            hftext_core_api_test
            hftext_wav_io_test
            hftext_streaming_receiver_test
            hftext_robust_test
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
    add_test(NAME hftext_core_api_test COMMAND hftext_core_api_test)
    add_test(NAME hftext_wav_io_test COMMAND hftext_wav_io_test)
    add_test(NAME hftext_streaming_receiver_test COMMAND hftext_streaming_receiver_test)
    add_test(NAME hftext_robust_test COMMAND hftext_robust_test)
    add_test(
        NAME hftext_cli_tx_wav_test
        COMMAND hftext_tx_wav
            --callsign pu5lrk
            --sample-rate 8000
            --symbol-duration 0.01
            --f0 1000
            --f1 2000
            "Teste"
            "$<TARGET_FILE_DIR:hftext_tx_wav>/ctest_roundtrip.wav"
    )
    add_test(
        NAME hftext_cli_rx_wav_test
        COMMAND hftext_rx_wav
            --verbose
            --symbol-duration 0.01
            --f0 1000
            --f1 2000
            "$<TARGET_FILE_DIR:hftext_tx_wav>/ctest_roundtrip.wav"
    )
    add_test(
        NAME hftext_cli_stream_wav_test
        COMMAND hftext_stream_wav
            --verbose
            --symbol-duration 0.01
            --f0 1000
            --f1 2000
            "$<TARGET_FILE_DIR:hftext_tx_wav>/ctest_roundtrip.wav"
    )
    add_test(
        NAME hftext_cli_tx_wav_4fsk_test
        COMMAND hftext_tx_wav
            --callsign pu5lrk
            --sample-rate 8000
            --symbol-duration 0.01
            --mode 4fsk
            --f0 1000
            --f1 1200
            "Teste"
            "$<TARGET_FILE_DIR:hftext_tx_wav>/ctest_roundtrip_4fsk.wav"
    )
    add_test(
        NAME hftext_cli_rx_wav_4fsk_test
        COMMAND hftext_rx_wav
            --verbose
            --symbol-duration 0.01
            --mode 4fsk
            --f0 1000
            --f1 1200
            "$<TARGET_FILE_DIR:hftext_tx_wav>/ctest_roundtrip_4fsk.wav"
    )
    add_test(
        NAME hftext_cli_stream_wav_4fsk_test
        COMMAND hftext_stream_wav
            --verbose
            --symbol-duration 0.01
            --mode 4fsk
            --f0 1000
            --f1 1200
            "$<TARGET_FILE_DIR:hftext_tx_wav>/ctest_roundtrip_4fsk.wav"
    )
    set_tests_properties(hftext_cli_tx_wav_test PROPERTIES
        PASS_REGULAR_EXPRESSION "Modo: robust"
    )
    set_tests_properties(hftext_cli_rx_wav_test PROPERTIES
        DEPENDS hftext_cli_tx_wav_test
        PASS_REGULAR_EXPRESSION "pu5lrk Teste"
    )
    set_tests_properties(hftext_cli_stream_wav_test PROPERTIES
        DEPENDS hftext_cli_tx_wav_test
        PASS_REGULAR_EXPRESSION "pu5lrk Teste"
    )
    set_tests_properties(hftext_cli_tx_wav_4fsk_test PROPERTIES
        PASS_REGULAR_EXPRESSION "Modo: robust-v0.2-exp-4fsk"
    )
    set_tests_properties(hftext_cli_rx_wav_4fsk_test PROPERTIES
        DEPENDS hftext_cli_tx_wav_4fsk_test
        PASS_REGULAR_EXPRESSION "pu5lrk Teste"
    )
    set_tests_properties(hftext_cli_stream_wav_4fsk_test PROPERTIES
        DEPENDS hftext_cli_tx_wav_4fsk_test
        PASS_REGULAR_EXPRESSION "pu5lrk Teste"
    )
endif()
