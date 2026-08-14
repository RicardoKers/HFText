cmake_minimum_required(VERSION 3.16)

find_package(Qt6 QUIET COMPONENTS Widgets)
if(NOT Qt6_FOUND)
    message(STATUS "Qt6 Widgets not found; pc-app will be skipped for this configuration.")
    return()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_executable(hftext_pc WIN32
    AudioInput.cpp
    AudioInput.h
    AudioOutput.cpp
    AudioOutput.h
    AudioSampleConverter.cpp
    AudioSampleConverter.h
    MainWindow.cpp
    MainWindow.h
    ModemController.cpp
    ModemController.h
    WaterfallWidget.cpp
    WaterfallWidget.h
    hftext_pc.rc
    main.cpp
    resources.qrc
)

target_compile_features(hftext_pc PRIVATE cxx_std_17)
target_compile_definitions(hftext_pc PRIVATE NOMINMAX)

target_link_libraries(hftext_pc
    PRIVATE
        Qt6::Widgets
        hftext_core
        hftext_wav_io
)

if(WIN32)
    target_link_libraries(hftext_pc PRIVATE ole32 uuid winmm)
endif()

if(HFTEXT_BUILD_CORE_TESTS)
    add_executable(hftext_pc_audio_converter_test
        AudioSampleConverter.cpp
        AudioSampleConverter.h
        tests/test_audio_sample_converter.cpp
    )
    target_compile_features(hftext_pc_audio_converter_test PRIVATE cxx_std_17)
    target_include_directories(hftext_pc_audio_converter_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME hftext_pc_audio_converter_test COMMAND hftext_pc_audio_converter_test)

    add_executable(hftext_pc_audio_input_devices_test
        AudioInput.cpp
        AudioInput.h
        AudioSampleConverter.cpp
        AudioSampleConverter.h
        tests/test_audio_input_devices.cpp
    )
    target_compile_features(hftext_pc_audio_input_devices_test PRIVATE cxx_std_17)
    target_include_directories(hftext_pc_audio_input_devices_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(hftext_pc_audio_input_devices_test PRIVATE hftext_wav_io)
    if(WIN32)
        target_link_libraries(hftext_pc_audio_input_devices_test PRIVATE ole32 uuid winmm)
    endif()
    add_test(NAME hftext_pc_audio_input_devices_test COMMAND hftext_pc_audio_input_devices_test)
endif()

if(MSVC)
    set_target_properties(hftext_pc PROPERTIES
        MSVC_DEBUG_INFORMATION_FORMAT Embedded
    )
    if(TARGET hftext_pc_audio_converter_test)
        set_target_properties(hftext_pc_audio_converter_test PROPERTIES
            MSVC_DEBUG_INFORMATION_FORMAT Embedded
        )
    endif()
    if(TARGET hftext_pc_audio_input_devices_test)
        set_target_properties(hftext_pc_audio_input_devices_test PROPERTIES
            MSVC_DEBUG_INFORMATION_FORMAT Embedded
        )
    endif()
endif()
