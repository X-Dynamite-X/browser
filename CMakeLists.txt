cmake_minimum_required(VERSION 3.16)
project(LiteBrowser VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LITEBROWSER_CORE_SOURCES
    src/core/fs_utils.cpp
    src/core/deflate.cpp
    src/core/zip_reader.cpp
    src/core/docx_reader.cpp
    src/core/doc_reader.cpp
    src/core/pdf_reader.cpp
    src/core/document_viewer.cpp
    src/core/url_utils.cpp
    src/core/storage.cpp
    src/core/internal_pages.cpp
)

add_library(litebrowser_core STATIC ${LITEBROWSER_CORE_SOURCES})
target_include_directories(litebrowser_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(UNIX AND NOT APPLE)
    target_link_libraries(litebrowser_core PUBLIC ${CMAKE_DL_LIBS} pthread)
endif()

if(WIN32)
    add_executable(litebrowser WIN32 src/platform/win32_main.cpp)
    target_link_libraries(litebrowser PRIVATE
        litebrowser_core
        comctl32
        comdlg32
        wininet
        ole32
        oleaut32
        uuid
        shell32
        shlwapi
        gdi32
        user32
        advapi32
    )
    if(MINGW)
        target_link_options(litebrowser PRIVATE -static -static-libgcc -static-libstdc++)
        option(TARGET_WINXP "Target Windows XP (NT 5.1) subsystem" OFF)
        if(TARGET_WINXP)
            target_compile_definitions(litebrowser PRIVATE _WIN32_WINNT=0x0501 WINVER=0x0501)
            target_link_options(litebrowser PRIVATE -Wl,--major-subsystem-version,5,--minor-subsystem-version,1)
        endif()
    endif()
elseif(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

    add_executable(litebrowser src/platform/linux_gtk_main.cpp)
    target_include_directories(litebrowser PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_libraries(litebrowser PRIVATE
        litebrowser_core
        ${GTK3_LIBRARIES}
        ${CMAKE_DL_LIBS}
    )
endif()

option(BUILD_LITEBROWSER_TESTS "Build LiteBrowser core unit tests" ON)
if(BUILD_LITEBROWSER_TESTS AND NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_executable(test_core tests/test_core.cpp)
    target_link_libraries(test_core PRIVATE litebrowser_core)
    add_test(NAME LiteBrowserCoreTests COMMAND test_core)
endif()

