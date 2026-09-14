cmake_minimum_required(VERSION 3.16)

project(Insta360Editor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INSTALL_RPATH "$ORIGIN")
set(CMAKE_BUILD_RPATH "$ORIGIN")

find_package(Qt5 REQUIRED COMPONENTS Widgets Concurrent)

add_executable(insta360_editor
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/Project.cpp
    src/Project.h
    src/ProjectDialog.cpp
    src/ProjectDialog.h
    src/TimelineWidget.cpp
    src/TimelineWidget.h
    src/VideoPreviewWidget.cpp
    src/VideoPreviewWidget.h
    src/ExportWorker.cpp
    src/ExportWorker.h
    src/SphericalMetadata.cpp
    src/SphericalMetadata.h
)

target_link_libraries(insta360_editor PRIVATE Qt5::Widgets Qt5::Concurrent)

set(INSTA360_MEDIASDK_ROOT "" CACHE PATH "Root path of the official Insta360 Desktop MediaSDK package")
set(INSTA360_MEDIASDK_INCLUDE_DIR "" CACHE PATH "Path to Insta360 MediaSDK include directory containing ins_stitcher.h")
set(INSTA360_MEDIASDK_LIBRARIES "" CACHE STRING "Libraries required to link Insta360 MediaSDK exporter")
set(INSTA360_MEDIASDK_LIBRARY_DIR "" CACHE PATH "Path containing Insta360 MediaSDK runtime libraries")
set(INSTA360_MEDIASDK_MODEL_DIR "" CACHE PATH "Path containing Insta360 MediaSDK AI models")
option(INSTA360_BUNDLE_MEDIASDK_RUNTIME "Copy Insta360 MediaSDK runtime libraries beside the application" OFF)
set(INSTA360_CAMERASDK_ROOT "" CACHE PATH "Root path of Insta360 CameraSDK 2.x")

# The official Linux ZIP contains headers outside its Debian runtime package.
# setup-local-sdk.sh extracts those two parts into these ignored directories.
set(_INSTA360_LOCAL_PACKAGE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/insta360-mediasdk-package")
set(_INSTA360_LOCAL_RUNTIME "${CMAKE_CURRENT_SOURCE_DIR}/vendor/insta360-mediasdk-runtime/opt/MediaSDK-3.1.5-linux")
set(_INSTA360_LOCAL_CAMERA "${CMAKE_CURRENT_SOURCE_DIR}/vendor/insta360-camerasdk")
if(NOT INSTA360_MEDIASDK_ROOT AND EXISTS "${_INSTA360_LOCAL_PACKAGE}/include/ins_stitcher.h")
    if(NOT INSTA360_MEDIASDK_INCLUDE_DIR)
        set(INSTA360_MEDIASDK_INCLUDE_DIR "${_INSTA360_LOCAL_PACKAGE}/include" CACHE PATH "" FORCE)
    endif()
    if(NOT INSTA360_MEDIASDK_LIBRARY_DIR)
        set(INSTA360_MEDIASDK_LIBRARY_DIR "${_INSTA360_LOCAL_RUNTIME}/lib" CACHE PATH "" FORCE)
    endif()
    if(NOT INSTA360_MEDIASDK_MODEL_DIR)
        set(INSTA360_MEDIASDK_MODEL_DIR "${_INSTA360_LOCAL_RUNTIME}/bin/models" CACHE PATH "" FORCE)
    endif()
endif()

if(INSTA360_MEDIASDK_ROOT)
    if(NOT INSTA360_MEDIASDK_INCLUDE_DIR)
        set(INSTA360_MEDIASDK_INCLUDE_DIR "${INSTA360_MEDIASDK_ROOT}/include" CACHE PATH "" FORCE)
    endif()
    if(NOT INSTA360_MEDIASDK_LIBRARY_DIR)
        if(EXISTS "${INSTA360_MEDIASDK_ROOT}/lib")
            set(INSTA360_MEDIASDK_LIBRARY_DIR "${INSTA360_MEDIASDK_ROOT}/lib" CACHE PATH "" FORCE)
        elseif(EXISTS "${INSTA360_MEDIASDK_ROOT}/bin")
            set(INSTA360_MEDIASDK_LIBRARY_DIR "${INSTA360_MEDIASDK_ROOT}/bin" CACHE PATH "" FORCE)
        endif()
    endif()
    if(NOT INSTA360_MEDIASDK_MODEL_DIR)
        if(EXISTS "${INSTA360_MEDIASDK_ROOT}/bin/models")
            set(INSTA360_MEDIASDK_MODEL_DIR "${INSTA360_MEDIASDK_ROOT}/bin/models" CACHE PATH "" FORCE)
        elseif(EXISTS "${INSTA360_MEDIASDK_ROOT}/models")
            set(INSTA360_MEDIASDK_MODEL_DIR "${INSTA360_MEDIASDK_ROOT}/models" CACHE PATH "" FORCE)
        endif()
    endif()
endif()

set(INSTA360_MEDIASDK_RUNTIME_FILES "")
if(INSTA360_MEDIASDK_LIBRARY_DIR)
    file(GLOB INSTA360_MEDIASDK_RUNTIME_FILES
        "${INSTA360_MEDIASDK_LIBRARY_DIR}/*.so"
        "${INSTA360_MEDIASDK_LIBRARY_DIR}/*.so.*"
        "${INSTA360_MEDIASDK_LIBRARY_DIR}/*.dll"
        "${INSTA360_MEDIASDK_LIBRARY_DIR}/*.dylib"
    )
endif()

if(NOT INSTA360_MEDIASDK_LIBRARIES AND INSTA360_MEDIASDK_LIBRARY_DIR)
    find_library(INSTA360_MEDIASDK_MAIN_LIBRARY
        NAMES MediaSDK
        PATHS "${INSTA360_MEDIASDK_LIBRARY_DIR}"
        NO_DEFAULT_PATH
    )
    if(INSTA360_MEDIASDK_MAIN_LIBRARY)
        set(INSTA360_MEDIASDK_LIBRARIES "${INSTA360_MEDIASDK_MAIN_LIBRARY}")
    endif()
endif()

install(TARGETS insta360_editor RUNTIME DESTINATION bin)

if(INSTA360_MEDIASDK_INCLUDE_DIR AND INSTA360_MEDIASDK_LIBRARIES)
    add_executable(insta360_sdk_exporter tools/insta360_sdk_exporter.cpp)
    target_include_directories(insta360_sdk_exporter PRIVATE "${INSTA360_MEDIASDK_INCLUDE_DIR}")
    target_link_libraries(insta360_sdk_exporter PRIVATE ${INSTA360_MEDIASDK_LIBRARIES})
    if(INSTA360_MEDIASDK_MODEL_DIR)
        target_compile_definitions(insta360_sdk_exporter PRIVATE
            INSTA360_DEFAULT_MODEL_DIR="${INSTA360_MEDIASDK_MODEL_DIR}")
    endif()
    target_compile_features(insta360_sdk_exporter PRIVATE cxx_std_17)
    set(_INSTA360_EXPORTER_INSTALL_RPATH "$ORIGIN")
    if(NOT INSTA360_BUNDLE_MEDIASDK_RUNTIME AND INSTA360_MEDIASDK_LIBRARY_DIR)
        list(APPEND _INSTA360_EXPORTER_INSTALL_RPATH "${INSTA360_MEDIASDK_LIBRARY_DIR}")
    endif()
    set_target_properties(insta360_sdk_exporter PROPERTIES
        BUILD_RPATH "$ORIGIN;${INSTA360_MEDIASDK_LIBRARY_DIR}"
        INSTALL_RPATH "${_INSTA360_EXPORTER_INSTALL_RPATH}"
    )
    add_custom_command(TARGET insta360_sdk_exporter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:insta360_sdk_exporter>
            $<TARGET_FILE_DIR:insta360_editor>
    )
    if(INSTA360_MEDIASDK_MODEL_DIR)
        add_custom_command(TARGET insta360_sdk_exporter POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f
                "$<TARGET_FILE_DIR:insta360_editor>/models"
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                "${INSTA360_MEDIASDK_MODEL_DIR}"
                "$<TARGET_FILE_DIR:insta360_editor>/models"
            VERBATIM
        )
    endif()
    if(INSTA360_BUNDLE_MEDIASDK_RUNTIME AND INSTA360_MEDIASDK_RUNTIME_FILES)
        add_custom_command(TARGET insta360_sdk_exporter POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${INSTA360_MEDIASDK_RUNTIME_FILES}
                $<TARGET_FILE_DIR:insta360_editor>
        )
        install(FILES ${INSTA360_MEDIASDK_RUNTIME_FILES} DESTINATION bin)
    endif()
    if(INSTA360_MEDIASDK_MODEL_DIR)
        install(DIRECTORY "${INSTA360_MEDIASDK_MODEL_DIR}/" DESTINATION bin/models)
    endif()
    install(TARGETS insta360_sdk_exporter RUNTIME DESTINATION bin)

    if(NOT INSTA360_CAMERASDK_ROOT AND EXISTS "${_INSTA360_LOCAL_CAMERA}/include/camera/camera.h")
        set(INSTA360_CAMERASDK_ROOT "${_INSTA360_LOCAL_CAMERA}")
    endif()
    find_library(INSTA360_CAMERASDK_LIBRARY NAMES CameraSDK
        PATHS "${INSTA360_CAMERASDK_ROOT}/lib" NO_DEFAULT_PATH)
    if(INSTA360_CAMERASDK_LIBRARY)
        target_sources(insta360_editor PRIVATE src/LiveCameraDialog.cpp src/LiveCameraDialog.h)
        target_include_directories(insta360_editor PRIVATE
            "${INSTA360_MEDIASDK_INCLUDE_DIR}" "${INSTA360_CAMERASDK_ROOT}/include")
        target_link_libraries(insta360_editor PRIVATE
            "${INSTA360_CAMERASDK_LIBRARY}" ${INSTA360_MEDIASDK_LIBRARIES})
        target_compile_definitions(insta360_editor PRIVATE
            INSTA360_HAS_LIVE_CAMERA=1 INSTA360_DEFAULT_MODEL_DIR="${INSTA360_MEDIASDK_MODEL_DIR}")
        set(_INSTA360_EDITOR_INSTALL_RPATH "$ORIGIN")
        if(NOT INSTA360_BUNDLE_MEDIASDK_RUNTIME)
            list(APPEND _INSTA360_EDITOR_INSTALL_RPATH
                "${INSTA360_CAMERASDK_ROOT}/lib" "${INSTA360_MEDIASDK_LIBRARY_DIR}")
        endif()
        set_target_properties(insta360_editor PROPERTIES
            BUILD_RPATH "${INSTA360_CAMERASDK_ROOT}/lib;${INSTA360_MEDIASDK_LIBRARY_DIR};$ORIGIN"
            INSTALL_RPATH "${_INSTA360_EDITOR_INSTALL_RPATH}")
        add_custom_command(TARGET insta360_editor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:insta360_editor>/jsons"
            COMMAND ${CMAKE_COMMAND} -E create_symlink "${INSTA360_CAMERASDK_ROOT}/bin/jsons"
                "$<TARGET_FILE_DIR:insta360_editor>/jsons")
        install(DIRECTORY "${INSTA360_CAMERASDK_ROOT}/bin/jsons/" DESTINATION bin/jsons)
        if(INSTA360_BUNDLE_MEDIASDK_RUNTIME)
            add_custom_command(TARGET insta360_editor POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${INSTA360_CAMERASDK_LIBRARY}"
                    "$<TARGET_FILE_DIR:insta360_editor>")
            install(FILES "${INSTA360_CAMERASDK_LIBRARY}" DESTINATION bin)
        endif()
        message(STATUS "Insta360 live CameraSDK + RealTimeStitcher enabled")
    else()
        message(STATUS "Live camera disabled: CameraSDK not found")
    endif()
else()
    message(STATUS "Insta360 MediaSDK exporter disabled. Set INSTA360_MEDIASDK_ROOT or INSTA360_MEDIASDK_INCLUDE_DIR/INSTA360_MEDIASDK_LIBRARIES to build and bundle it.")
endif()
