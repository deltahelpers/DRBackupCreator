cmake_minimum_required(VERSION 3.20)
project(DRBackupCreator CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# temporary core until i move it to another repo (.dll)
add_library(DRBackupCCore SHARED
    src/core.cpp
    src/paths.cpp
    src/slots.cpp
    src/process.cpp
)
target_include_directories(DRBackupCCore PUBLIC include)
target_compile_definitions(DRBackupCCore PRIVATE DRBACKUP_BUILD)

if(WIN32)
    set_target_properties(DRBackupCCore PROPERTIES PREFIX "")
endif()

# the GUI of the application (Dear ImGUI)
add_executable(DRBackupCreator
    src/main.cpp      
    src/cli.cpp       
    src/gui.cpp       
    include/imgui/imgui.cpp
    include/imgui/imgui_draw.cpp
    include/imgui/imgui_tables.cpp
    include/imgui/imgui_widgets.cpp
    include/imgui/imgui_demo.cpp
    include/imgui/backends/imgui_impl_glfw.cpp
    include/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(DRBackupCreator PRIVATE
    include
    include/imgui
    deps/glfw/include
)

target_link_libraries(DRBackupCreator PRIVATE
    DRBackupCCore
    ${CMAKE_SOURCE_DIR}/deps/glfw/lib/libglfw3.a
    opengl32
    gdi32
    user32
    shell32
)