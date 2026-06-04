cmake_minimum_required(VERSION 3.16)
project(SteamLauncher VERSION 1.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows 特定设置
if(WIN32)
    # 启用 Unicode
    add_definitions(-DUNICODE -D_UNICODE)
    # 链接 Windows 库
    set(PLATFORM_LIBS advapi32 shell32)
endif()

# 源文件
set(SOURCES
    src/main.cpp
    src/vdf_parser.cpp
    src/steam_scanner.cpp
    src/game_launcher.cpp
    src/category_manager.cpp
    src/playtime_tracker.cpp
    src/console_ui.cpp
)

# 头文件
set(HEADERS
    src/vdf_parser.h
    src/game_data.h
    src/steam_scanner.h
    src/game_launcher.h
    src/category_manager.h
    src/playtime_tracker.h
    src/console_ui.h
)

# 可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(${PROJECT_NAME} PRIVATE ${PLATFORM_LIBS})

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE src)

# 编译选项
if(MSVC)
    # MSVC 特定选项
    target_compile_options(${PROJECT_NAME} PRIVATE /W3 /utf-8)
    # /utf-8: 源文件和执行字符集都使用 UTF-8
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

# 设置输出目录
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)

# 安装规则
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
