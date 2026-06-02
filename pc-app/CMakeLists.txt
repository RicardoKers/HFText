cmake_minimum_required(VERSION 3.16)

find_package(Qt6 QUIET COMPONENTS Widgets)
if(NOT Qt6_FOUND)
    message(STATUS "Qt6 Widgets nao encontrado; pc-app sera ignorado nesta configuracao.")
    return()
endif()

set(CMAKE_AUTOMOC ON)

add_executable(hftext_pc
    AudioInput.cpp
    AudioInput.h
    AudioOutput.cpp
    AudioOutput.h
    MainWindow.cpp
    MainWindow.h
    ModemController.cpp
    ModemController.h
    main.cpp
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
    target_link_libraries(hftext_pc PRIVATE winmm)
endif()

if(MSVC)
    set_target_properties(hftext_pc PROPERTIES
        MSVC_DEBUG_INFORMATION_FORMAT Embedded
    )
endif()
