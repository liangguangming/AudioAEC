cmake_minimum_required(VERSION 3.16)
project(audioAEC VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 源文件
set(SOURCES
    src/main.cpp
    src/AudioAECWrapper.cpp
    src/AudioAECImpl.mm
)

# 头文件
set(HEADERS
    src/AudioAECWrapper.h
    src/AudioAECImpl.h
)

# 创建可执行文件
add_executable(audioAEC ${SOURCES} ${HEADERS})

# 设置包含目录
target_include_directories(audioAEC PRIVATE src)

# 查找并链接macOS框架
find_library(AUDIO_TOOLBOX_FRAMEWORK AudioToolbox)
find_library(CORE_AUDIO_FRAMEWORK CoreAudio)
find_library(AUDIO_UNIT_FRAMEWORK AudioUnit)

# 链接框架
target_link_libraries(audioAEC 
    ${AUDIO_TOOLBOX_FRAMEWORK}
    ${CORE_AUDIO_FRAMEWORK}
    ${AUDIO_UNIT_FRAMEWORK}
)

# 设置编译选项
if(APPLE)
    target_compile_options(audioAEC PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fobjc-arc
    )
    
    # 设置Objective-C++编译选项
    set_source_files_properties(src/AudioAECImpl.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
endif()

# 安装规则
install(TARGETS audioAEC
    RUNTIME DESTINATION bin
)

# 打印构建信息
message(STATUS "Building audioAEC project")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}") 