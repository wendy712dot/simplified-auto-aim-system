cmake_minimum_required(VERSION 3.8)
project(auto_aim_ros2)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(
  image_subscriber_test
  src/image_subscriber_test.cpp
)

ament_target_dependencies(
  image_subscriber_test
  rclcpp
  sensor_msgs
  cv_bridge
)

target_include_directories(
  image_subscriber_test
  PRIVATE
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(
  image_subscriber_test
  ${OpenCV_LIBS}
)

install(
  TARGETS image_subscriber_test
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()