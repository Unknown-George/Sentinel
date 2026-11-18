cmake_minimum_required(VERSION 3.24)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Apple Silicon")
endif()
project(Sentinel VERSION 1.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt 6.8's yield helper needs the ARM intrinsic declaration with recent Apple Clang.
if(APPLE AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-include>"
                        "$<$<COMPILE_LANGUAGE:CXX,OBJCXX>:arm_acle.h>")
endif()

option(SENTINEL_BUILD_APP "Build native macOS application" ${APPLE})
option(SENTINEL_BUILD_TESTS "Build GoogleTest unit tests" ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core)
find_package(SQLite3 REQUIRED)

add_library(sentinel_core STATIC src/core/HistoryStore.cpp)
target_include_directories(sentinel_core PUBLIC src)
if(TARGET SQLite3::SQLite3)
    set(SENTINEL_SQLITE_TARGET SQLite3::SQLite3)
else()
    set(SENTINEL_SQLITE_TARGET SQLite::SQLite3)
endif()
target_link_libraries(sentinel_core PUBLIC Qt6::Core ${SENTINEL_SQLITE_TARGET})
target_compile_options(sentinel_core PRIVATE -Wall -Wextra -Wpedantic)

if(SENTINEL_BUILD_APP)
    if(NOT APPLE)
        message(FATAL_ERROR "The monitoring collector requires macOS 14+ on Apple Silicon.")
    endif()
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 20)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg)
    # Older Qt SDKs propagate an unused AGL link requirement to consumers. Recent
    # Apple SDKs omit its development stub. Sentinel uses raster Widgets and no AGL
    # symbols; QtGui retains responsibility for its own runtime dependencies.
    execute_process(COMMAND xcrun --show-sdk-path OUTPUT_VARIABLE SENTINEL_APPLE_SDK OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(Qt6_VERSION VERSION_LESS "6.9" AND TARGET WrapOpenGL::WrapOpenGL AND
       NOT EXISTS "${SENTINEL_APPLE_SDK}/System/Library/Frameworks/AGL.framework")
        get_target_property(SENTINEL_OPENGL_LIBS WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
        list(FILTER SENTINEL_OPENGL_LIBS EXCLUDE REGEX "AGL")
        set_property(TARGET WrapOpenGL::WrapOpenGL PROPERTY INTERFACE_LINK_LIBRARIES "${SENTINEL_OPENGL_LIBS}")
    endif()
    file(GLOB SENTINEL_UI_SOURCES CONFIGURE_DEPENDS src/ui/*.cpp src/ui/*.h)
    file(GLOB SENTINEL_RESOURCE_FILES CONFIGURE_DEPENDS resources/*.qrc)
    qt_add_executable(Sentinel MACOSX_BUNDLE
        src/main.cpp src/core/MonitorService.h src/core/MonitorService.cpp
        src/platform/Collector.mm ${SENTINEL_UI_SOURCES} ${SENTINEL_RESOURCE_FILES})
    set_source_files_properties(src/platform/Collector.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_include_directories(Sentinel PRIVATE src)
    target_compile_definitions(Sentinel PRIVATE SENTINEL_VERSION="${PROJECT_VERSION}")
    target_link_libraries(Sentinel PRIVATE sentinel_core Qt6::Widgets Qt6::Svg
        "-framework Foundation" "-framework AppKit" "-framework Metal"
        "-framework IOKit" "-framework Security")
    target_compile_options(Sentinel PRIVATE -Wall -Wextra -Wpedantic)
    set_target_properties(Sentinel PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.sentinel.monitor"
        MACOSX_BUNDLE_BUNDLE_NAME "Sentinel"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/Info.plist.in")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/Sentinel.icns")
        target_sources(Sentinel PRIVATE resources/Sentinel.icns)
        set_source_files_properties(resources/Sentinel.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    endif()
    install(TARGETS Sentinel BUNDLE DESTINATION .)
endif()

if(SENTINEL_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    file(GLOB SENTINEL_TEST_SOURCES CONFIGURE_DEPENDS tests/*Test.cpp)
    add_executable(sentinel_tests ${SENTINEL_TEST_SOURCES} src/ui/ProcessModel.cpp)
    target_link_libraries(sentinel_tests PRIVATE sentinel_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(sentinel_tests)
endif()
