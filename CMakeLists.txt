cmake_minimum_required(VERSION 3.10)

project(simplified_auto_aim)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 OpenCV
find_package(OpenCV REQUIRED)

# 生成可执行程序
add_executable(
    auto_aim
    src/main.cpp
    src/video_reader.cpp
    src/armor_detector.cpp
    src/pose_solver.cpp
    src/config.cpp
    src/target_selector.cpp
    src/debug_visualizer.cpp
)

# 告诉编译器去哪里找头文件
target_include_directories(
    auto_aim
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# 将 OpenCV 链接到程序
target_link_libraries(
    auto_aim
    PRIVATE
    ${OpenCV_LIBS}
)