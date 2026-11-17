cmake_minimum_required(VERSION 3.20)

project(DRBackupCreator VERSION 1.0.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------------------
# Core shared library (DRBackupCCore.dll)
# ---------------------------------------------------------------------------
add_library(DRBackupCCore SHARED
    src/core.cpp
    src/paths.cpp
    src/slots.cpp
    src/process.cpp
)

target_include_directories(DRBackupCCore PUBLIC include)
target_compile_definitions(DRBackupCCore PRIVATE DRBACKUP_BUILD)
target_compile_definitions(DRBackupCCore PUBLIC
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00 WINVER=0x0A00)

if(WIN32)
    set_target_properties(DRBackupCCore PROPERTIES PREFIX "")
endif()

# ---------------------------------------------------------------------------
# Win32 GUI + CLI executable
# ---------------------------------------------------------------------------
add_executable(drbc WIN32
    src/main.cpp
    src/cli.cpp
    src/ui/gui.cpp
    src/ui/dialogs.cpp
    src/ui/theme.cpp
    src/ui/i18n.cpp
    src/ui/app.rc
)

target_include_directories(drbc PRIVATE include src)
target_compile_definitions(drbc PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    _WIN32_WINNT=0x0A00 WINVER=0x0A00)

target_link_libraries(drbc PRIVATE
    DRBackupCCore
    comctl32
    shell32
    dwmapi
    uxtheme
    ole32
    shlwapi
)

if(MINGW)
    target_link_options(drbc PRIVATE -municode)
endif()

# ---------------------------------------------------------------------------
# Tests (core only, no GUI)
# ---------------------------------------------------------------------------
enable_testing()

add_executable(drbc_tests tests/core_tests.cpp)
target_include_directories(drbc_tests PRIVATE include)
target_compile_definitions(drbc_tests PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_link_libraries(drbc_tests PRIVATE DRBackupCCore)

add_test(NAME drbc_core COMMAND drbc_tests)

# ---------------------------------------------------------------------------
# Install / package (ready for the future installer)
# ---------------------------------------------------------------------------
install(TARGETS drbc DRBackupCCore RUNTIME DESTINATION .)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "DRBackupCreator")
set(CPACK_PACKAGE_VENDOR "DeltaHelpers")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "DRBackupCreator-${PROJECT_VERSION}-win32")
include(CPack)
