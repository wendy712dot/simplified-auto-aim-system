# 仿真部署与接口记录

## 1. 环境与版本

| 项目 | 配置 |
|---|---|
| 操作系统 | Ubuntu 22.04.5 LTS |
| ROS2 | Humble |
| OpenCV | 4.5.4 |
| CMake | 3.22.1 |
| G++ | 11.4.0 |
| 仿真器 Rust | 1.95.0 |
| GPU | NVIDIA RTX 1000 Ada |
| NVIDIA 驱动 | 595.91.07 |

仿真器：

```text
HNUYueLuRM/bevy_robomaster_simulator
```

使用的原始提交：

```text
479d8fd7d40863dbdc2e021f58ad297f9df9f8e9
```

仿真器 ROS2 修复分支和提交：

```text
fix/ros2-interface-compatibility
38228c7
```

原自瞄项目开发分支：

```text
feature/simulator-integration
```

---

## 2. 仿真器启动

加载环境：

```bash
source /opt/ros/humble/setup.bash
source ~/Projects/ros2_ws/install/setup.bash
```

启动 ROS2 版本仿真器：

```bash
cd ~/Projects/bevy_robomaster_simulator
cargo run --release --no-default-features --features ros2
```

仿真器节点：

```text
/robomaster/simulator
```

建议截图：

```markdown
![仿真器ROS2运行及话题列表](images/simulator_ros2_topics.png)
```

截图中保留仿真窗口以及 `ros2 node list`、`ros2 topic list` 的关键输出即可。

---

## 3. ROS2 兼容修复

仿真器源码原本引用 `hnurm_interfaces`，但仓库实际提供的消息包为
`rm_interfaces`，消息名称和部分字段形式也不一致，导致 ROS2 feature
无法编译。

主要修改：

- 将 `hnurm_interfaces` 改为 `rm_interfaces`。
- 将 `VisionRecvData` 对应到 `VisionReceiveData`。
- 按实际消息定义调整 `uint8` 字段。
- 删除当前消息中不存在的字段。
- 将 `target_state.data` 改为 `target_state`。

修改后 ROS2 feature 可以正常编译和运行。

---

## 4. 接口记录

### 4.1 图像

| 项目 | 结果 |
|---|---|
| 话题 | `/image_raw` |
| 消息类型 | `sensor_msgs/msg/Image` |
| 编码 | `rgb8` |
| 分辨率 | 1440 × 1080 |
| 实际频率 | 约 59～60 FPS |
| frame_id | `camera_optical_frame` |
| QoS | RELIABLE、VOLATILE |

OpenCV 默认使用 BGR，因此订阅后通过 `cv_bridge` 将：

```text
rgb8 → BGR8
```

订阅队列深度设置为 1，只保留最新画面，避免积压旧帧。

### 4.2 相机参数

话题：

```text
/camera_info
```

内参：

```text
fx = 1303.6753
fy = 1303.6753
cx = 720.0
cy = 540.0
```

相机矩阵：

```text
[1303.6753, 0,         720]
[0,         1303.6753, 540]
[0,         0,         1  ]
```

畸变参数均为 0，内参对应 1440×1080 图像。后续 PnP 使用该参数，
不能直接沿用原视频参数。

### 4.3 云台命令

| 项目 | 结果 |
|---|---|
| 话题 | `/vision_send_data` |
| 消息类型 | `rm_interfaces/msg/VisionSendData` |
| 接口方向 | 自瞄程序发布，仿真器订阅 |
| QoS | BEST_EFFORT、VOLATILE |
| 角度单位 | 度 |
| 控制语义 | 世界参考下的绝对目标角度 |

方向关系：

```text
yaw 增大   → 云台向左
yaw 减小   → 云台向右
pitch 增大 → 云台向下
pitch 减小 → 云台向上
```

目标状态：

```text
target_state = 0：目标无效，停止追踪并保持当前姿态
target_state = 1：目标有效，不开火
target_state = 2：目标有效，并给出开火建议
```

`target_distance=-1` 也会被判断为无效目标。

### 4.4 云台状态

| 项目 | 结果 |
|---|---|
| 话题 | `/vision_receive_data` |
| 消息类型 | `rm_interfaces/msg/VisionReceiveData` |
| 接口方向 | 仿真器发布，自瞄程序订阅 |
| 发布频率 | 约 60 Hz |
| 当前弹速 | 25 m/s |

消息中包含当前 roll、pitch、yaw 和机器人状态。

### 4.5 坐标关系

`/gimbal_pose`、`/camera_pose` 和 `/odom_pose` 当前只发布恒等占位
Pose。实际坐标关系位于 `/tf`：

```text
map
└── odom
    └── gimbal_link
        ├── muzzle
        │   └── muzzle_link
        └── camera_link
            └── camera_optical_frame
```

第一版最小闭环直接使用 `/vision_receive_data` 获取云台角度，不进行复杂
TF 解算。

---

## 5. 最小通信测试

### 5.1 图像订阅测试

程序：

```text
ros2/auto_aim_ros2/src/image_subscriber_test.cpp
```

运行：

```bash
ros2 run auto_aim_ros2 image_subscriber_test
```

测试结果：

- 能够显示仿真实时画面。
- 图像为 1440×1080、`rgb8`。
- RGB 转 BGR 后颜色正常。
- 接收 FPS 稳定在约 59～60。
- 能输出时间戳。
- 移动机器人或云台时，画面实时变化。
- 按 `Q` 或 `Esc` 可以正常退出。

该测试证明：

```text
仿真器 → ROS2 → C++/OpenCV程序
```

已经连通。

建议截图：

```markdown
![图像订阅测试](images/image_subscriber_test.png)
```

截图中同时保留 OpenCV 画面和终端 FPS。

### 5.2 云台命令测试

程序：

```text
ros2/auto_aim_ros2/src/gimbal_command_test.cpp
```

运行示例：

```bash
ros2 run auto_aim_ros2 gimbal_command_test \
  --ros-args \
  -p yaw:=10.0 \
  -p pitch:=0.0
```

测试结果：

- C++ 程序能够向仿真器发布云台命令。
- 云台能够逐渐运动到指定角度。
- 状态话题反馈角度与命令一致。
- yaw、pitch 正负方向符合接口记录。
- `target_state=0` 时不执行消息中的角度，并保持当前姿态。

该测试证明：

```text
C++程序 → ROS2 → 仿真器云台
```

已经连通。

建议截图：

```markdown
![云台命令测试](images/gimbal_command_test.png)
```

截图中保留命令参数、仿真画面和角度反馈即可。

---

## 6. 数据流

```text
仿真虚拟相机
    ↓ /image_raw
图像订阅程序
    ↓ cv::Mat
后续自瞄算法
    ↓ AimResult
云台输出程序
    ↓ /vision_send_data
仿真云台运动
    ↓
产生新的相机画面
```

后续形成完整系统图后，可在这里增加一张数据流图：

```markdown
![系统数据流](images/system_data_flow.png)
```

---

## 7. 主要问题与解决方法

| 问题 | 解决方法 |
|---|---|
| NVIDIA 驱动安装后未立即生效 | 重启 Ubuntu |
| 旧终端找不到 `ros2` | 执行 `source ~/.bashrc` |
| F3 被识别为音量键 | 使用 `Fn+F3` 或切换 Fn Lock |
| 方向键不能手动控制云台 | 将 `auto-aim` 切换为 OFF |
| ROS2 feature 无法编译 | 修正消息包名称、消息类型及字段 |
| `/gimbal_pose` 等始终为零 | 使用 `/tf` 或 `/vision_receive_data` |

---

## 8. 当前结论

目前已经完成：

```text
仿真器部署
→ ROS2兼容修复
→ 接口确认
→ 图像订阅测试
→ 云台命令发布测试
```

图像输入和云台输出两条通信链路均已打通。

下一阶段将完成：

```text
ROS2 Image
→ cv::Mat
→ 原装甲板检测
→ 目标选择
→ PnP
→ yaw / pitch / distance / target_valid
```

下一阶段先保持开环，只运行实时视觉算法，不立即使用检测结果控制云台。