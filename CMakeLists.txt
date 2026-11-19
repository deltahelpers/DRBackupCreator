cmake_minimum_required(VERSION 3.20)

project(DRBackupCreator VERSION 2.2.0 LANGUAGES CXX RC)

# CI can stamp a release version without editing this file:
#   cmake -B build -DDRBC_RELEASE_VERSION=2.0.0
if(NOT "${DRBC_RELEASE_VERSION}" STREQUAL "")
    set(PROJECT_VERSION "${DRBC_RELEASE_VERSION}")
    # project() is what normally fills the component variables; keep them in
    # step with the override so the generated version header and .rc agree.
    string(REPLACE "." ";" _drbc_ver "${PROJECT_VERSION}")
    list(LENGTH _drbc_ver _drbc_ver_len)
    list(GET _drbc_ver 0 PROJECT_VERSION_MAJOR)
    set(PROJECT_VERSION_MINOR 0)
    set(PROJECT_VERSION_PATCH 0)
    if(_drbc_ver_len GREATER 1)
        list(GET _drbc_ver 1 PROJECT_VERSION_MINOR)
    endif()
    if(_drbc_ver_len GREATER 2)
        list(GET _drbc_ver 2 PROJECT_VERSION_PATCH)
    endif()
    unset(_drbc_ver)
    unset(_drbc_ver_len)
endif()

# Generated version header + version resource, so the version baked into the
# binaries always matches the ZIP and the installers.
set(DRBC_RC_VERSION "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")
set(DRBC_RC_VERSION_STR "${PROJECT_VERSION}.0")
set(DRBC_GEN_DIR "${CMAKE_BINARY_DIR}/generated")
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" DRBC_SRC_POSIX)
configure_file(include/version.h.in "${DRBC_GEN_DIR}/version.h" @ONLY)
configure_file(src/ui/app.rc.in "${DRBC_GEN_DIR}/app.rc" @ONLY)

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
    src/config.cpp
    src/schedule.cpp
    src/settings.cpp
    src/update.cpp
    src/update_parse.cpp
    src/ui/gui.cpp
    src/ui/dialogs.cpp
    src/ui/update_ui.cpp
    src/ui/theme.cpp
    src/ui/i18n.cpp
    ${DRBC_GEN_DIR}/app.rc
)

# BEFORE: the generated version.h must win over anything else on the path.
target_include_directories(drbc BEFORE PRIVATE "${DRBC_GEN_DIR}")
target_include_directories(drbc PRIVATE include src)
target_compile_definitions(drbc PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    _WIN32_WINNT=0x0A00 WINVER=0x0A00)

target_link_libraries(drbc PRIVATE
    DRBackupCCore
    comctl32
    comdlg32
    shell32
    dwmapi
    uxtheme
    ole32
    shlwapi
    winhttp
    bcrypt
)

if(MINGW)
    # Static MinGW runtime: MSYS2's GCC links libstdc++-6.dll, libgcc_s_seh-1.dll
    # and libwinpthread-1.dll dynamically by default, which would leave the
    # shipped binaries broken on a clean Windows install. TDM-GCC already links
    # statically; -static keeps CI builds identical.
    target_link_options(drbc PRIVATE -municode -static)
    target_link_options(DRBackupCCore PRIVATE -static)
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

if(MINGW)
    target_link_options(drbc_tests PRIVATE -static)
endif()

# .bctheme / .bcconfig reader: pure logic, no DLL and no window.
add_executable(drbc_config_tests tests/config_tests.cpp src/config.cpp)
target_include_directories(drbc_config_tests PRIVATE src)
target_compile_definitions(drbc_config_tests PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00 WINVER=0x0A00)
add_test(NAME drbc_config COMMAND drbc_config_tests)

if(MINGW)
    target_link_options(drbc_config_tests PRIVATE -static)
endif()

# Update checker: version compare, release JSON and the notes cleaner.
add_executable(drbc_update_tests tests/update_tests.cpp src/update_parse.cpp)
target_include_directories(drbc_update_tests PRIVATE src)
target_compile_definitions(drbc_update_tests PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00 WINVER=0x0A00)
add_test(NAME drbc_update COMMAND drbc_update_tests)

if(MINGW)
    target_link_options(drbc_update_tests PRIVATE -static)
endif()

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
