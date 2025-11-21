#=====================================================================
# CMakeLists.txt for ckw
#=====================================================================
cmake_minimum_required(VERSION 3.20)

#------------------------------------------
# Porject setting
#------------------------------------------
project(ckw LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#------------------------------------------
# Generate version.h
#------------------------------------------
set(VERSION_MAJOR 0)
set(VERSION_MINOR 9)
set(VERSION_BUGFIX 1)
set(VERSION_NUM 00901)

# Build date
string(TIMESTAMP BUILDDATE "%Y-%m-%d %H:%M:%S UTC+9")
string(TIMESTAMP CURRENT_YEAR "%Y")
set(COPYRIGHT_YEAR "${CURRENT_YEAR}")  # 2025 など

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    @ONLY
)

#------------------------------------------
# Source
#------------------------------------------
set(SRC
    main.cpp
    selection.cpp
    misc.cpp
    option.cpp
    encoding.cpp
    winmng.cpp
    rsrc.rc
    ckw.manifest
)

#------------------------------------------
# 実行ファイル作成
#------------------------------------------
add_executable(ckw WIN32 ${SRC})

# 生成されたヘッダーをインクルードディレクトリに追加
target_include_directories(ckw PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

#------------------------------------------
# Compile Option
#------------------------------------------
target_compile_options(ckw PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:
        $<$<CONFIG:Release>:/MT /W3 /EHsc>
        $<$<CONFIG:Debug>:/MDd /W3 /EHsc>
    >
)

target_compile_definitions(ckw PRIVATE
    UNICODE
    _UNICODE
    WINVER=0x0606
    $<$<CXX_COMPILER_ID:MSVC>:
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
    >
)

#------------------------------------------
# Link
#------------------------------------------
target_link_libraries(ckw PRIVATE
    user32
    gdi32
    shell32
    shlwapi
    imm32
    advapi32
    dwmapi
    uxtheme
)

#------------------------------------------
# Output folder
#------------------------------------------
set_target_properties(ckw PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

#------------------------------------------
# Information
#------------------------------------------
message(STATUS "Version: ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_BUGFIX}")
message(STATUS "Copyright year: ${COPYRIGHT_YEAR}")

