# Simplified Auto Aim System

一个基于 C++ 和 OpenCV 实现的简化装甲板自动瞄准系统。

项目以测试视频作为输入，完成图像预处理、灯条检测、装甲板匹配、目标选择和 PnP 位姿解算，并在运行窗口中显示目标位置、Yaw、Pitch、距离、FPS 和目标状态。

当前项目主要用于完成自动瞄准视觉流程的基础实现，重点是建立完整、模块化且便于调试的处理流程。

---

## 1. 实现流程

程序整体处理流程如下：

```text
读取视频
   ↓
图像预处理
   ↓
轮廓提取
   ↓
灯条筛选
   ↓
灯条配对
   ↓
生成装甲板候选
   ↓
选择当前目标
   ↓
solvePnP 位姿解算
   ↓
计算 Yaw / Pitch / Distance
   ↓
显示并保存结果
```

### 图像预处理

程序支持灰度阈值和颜色差分两种预处理方式。

当前测试视频使用蓝色目标，因此采用：

```text
B - R
```

进行颜色差分，再通过阈值二值化突出蓝色区域。

### 灯条检测

在二值图中提取轮廓，并使用 `minAreaRect` 得到旋转矩形。

根据以下条件筛选灯条：

- 最小面积
- 长宽比
- 倾斜角度

这样可以过滤较小的噪声以及明显的水平干扰区域。

### 装甲板匹配

将筛选后的灯条两两组合，并根据几何关系判断是否能够构成装甲板，主要考虑：

- 两灯条高度差异
- 两灯条中心高度差
- 装甲板整体宽高比

满足条件的灯条对会生成一个装甲板候选。

### 目标选择

当一帧中存在多个候选目标时，程序会选择一个作为当前目标。

首次检测时优先选择靠近图像中心的候选；已经存在历史目标时，则优先选择距离上一帧目标中心最近的候选，从而减少目标在多个候选之间频繁跳变。

目标状态分为：

```text
NO_TARGET
DETECTED
TRACKING
TEMP_LOST
```

短时间检测不到目标时不会立即清除跟踪状态，只有连续丢失超过设定帧数后才进入 `NO_TARGET`。

### 位姿解算

程序根据装甲板四个角点的二维坐标和装甲板实际尺寸，通过 OpenCV `solvePnP` 计算目标相对于相机的三维位置，并进一步得到：

```text
Yaw
Pitch
Distance
```

当前没有使用真实相机标定结果，而是根据图像尺寸构造近似相机内参，因此位姿数据主要用于验证完整的 PnP 解算流程，不能作为高精度测量结果。

---

## 2. 项目结构

```text
simplified-auto-aim-system/
├── CMakeLists.txt
├── config/
│   └── config.yaml
├── include/
│   ├── video_reader.hpp
│   ├── armor_detector.hpp
│   ├── target_selector.hpp
│   ├── pose_solver.hpp
│   ├── debug_visualizer.hpp
│   └── config.hpp
├── src/
│   ├── main.cpp
│   ├── video_reader.cpp
│   ├── armor_detector.cpp
│   ├── target_selector.cpp
│   ├── pose_solver.cpp
│   ├── debug_visualizer.cpp
│   └── config.cpp
├── videos/
│   └── test.mp4
└── output/
    └── result.avi
```

各模块职责如下：

| 模块 | 作用 |
| --- | --- |
| `VideoReader` | 读取测试视频和视频 FPS |
| `ArmorDetector` | 图像预处理、灯条筛选和装甲板匹配 |
| `TargetSelector` | 目标选择、简单目标保持和状态管理 |
| `PoseSolver` | solvePnP 位姿解算，计算 Yaw、Pitch 和距离 |
| `DebugVisualizer` | 显示中间结果、最终结果并保存输出视频 |
| `Config` | 从 YAML 文件读取检测和调试参数 |

`main.cpp` 只负责组织各模块的调用，不直接实现具体的检测算法。

---

## 3. 编译与运行

项目测试环境：

```text
Ubuntu 22.04
C++17
OpenCV 4.5.4
CMake
```

进入项目目录并编译：

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

运行：

```bash
./auto_aim
```

默认输入视频：

```text
videos/test.mp4
```

处理后的结果视频保存在：

```text
output/result.avi
```

输出视频使用 MJPG 编码的 AVI 格式，以保证当前 Ubuntu/OpenCV 环境下的播放兼容性。

---

## 4. 参数配置

主要参数统一放在：

```text
config/config.yaml
```

当前使用的主要参数包括：

```yaml
enemy:
  color: "blue"

preprocess:
  method: "color_difference"
  binary_threshold: 150

light_bar:
  min_area: 10.0
  min_ratio: 1.6
  min_angle: 60.0

armor:
  max_height_ratio: 1.5
  max_y_ratio: 0.7
  min_armor_aspect_ratio: 1.5
  max_armor_aspect_ratio: 5.0

target:
  tracking_threshold: 3
  temporary_lost_threshold: 5
```

其中 `min_ratio` 和 `max_y_ratio` 根据当前测试视频进行了适当调整，以减少目标发生倾斜、运动或发光区域变化时的漏检。

相机参数、装甲板实际尺寸以及调试窗口开关也可以在该文件中修改。

---

## 5. 调试与输出

程序可以显示以下窗口：

```text
Binary
All Rotated Rects
Filtered Light Bars
Auto Aim Result
```

这些窗口可以通过 `config.yaml` 中的 `debug` 参数分别开启或关闭。

最终结果窗口中会显示：

- 装甲板候选框
- 当前选中的目标
- Yaw
- Pitch
- Distance
- FPS
- Target Status

同时程序会将最终结果保存为 `output/result.avi`，便于运行结束后检查完整检测效果。

### 运行截图

![运行效果](docs/result.png)

### 运行视频

完整运行结果保存在：

[`output/result.avi`](output/result.avi)

视频记录了装甲板检测、目标选择、位姿解算以及目标状态变化的完整过程。

---

## 6. 当前效果与不足

目前已经完成从视频输入到目标位姿输出的完整基础流程，在提供的测试视频上能够较稳定地检测和跟踪蓝色装甲板，并对短时间目标丢失进行简单处理。

当前仍有以下限制：

- 检测参数主要针对现有测试视频调整，环境变化较大时需要重新调参。
- 灯条端点根据近似竖直灯条计算，目标发生较大倾斜时精度会下降。
- 当前使用近似相机内参，Yaw、Pitch 和距离只用于验证位姿解算流程。
- `camera.use_approximate_intrinsics` 目前作为后续接入真实相机标定参数的预留配置，暂未实现内参模式切换。
- 当前目标保持采用上一帧目标中心和丢失计数器，没有加入运动预测等复杂跟踪算法。

后续如果接入真实相机，可以在现有结构上继续加入相机标定参数、实时视频输入以及更稳定的目标跟踪方法。