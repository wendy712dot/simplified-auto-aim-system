#include <iostream>
#include <chrono>

#include <opencv2/opencv.hpp>

#include "video_reader.hpp"
#include "armor_detector.hpp"
#include "pose_solver.hpp"
#include "config.hpp"
#include "target_selector.hpp"
#include "debug_visualizer.hpp"


int main()
{
    // ==========================================
    // 0. 加载配置文件
    // ==========================================

    Config config = createDefaultConfig();

    if (!loadConfig("../config/config.yaml", config))
    {
        std::cerr
            << "Failed to load config.yaml, "
            << "using default configuration."
            << std::endl;
    }
    else
    {
        std::cout
            << "Config loaded successfully!"
            << std::endl;
    }


    // ==========================================
    // 1. 打开测试视频
    // ==========================================

    VideoReader reader("../videos/test.mp4");

    if (!reader.isOpened())
    {
        std::cerr
            << "Failed to open video!"
            << std::endl;

        return -1;
    }


    // ==========================================
    // 2. 创建各功能模块
    // ==========================================

    ArmorDetector detector(
        config.enemy,
        config.preprocess,
        config.light_bar,
        config.armor
    );

    TargetSelector selector(
        config.target
    );

    PoseSolver pose_solver(
        config.camera,
        config.armor_size
    );

    DebugVisualizer visualizer(
        config.debug,
        reader.getFPS()
    );

    std::cout
        << "Video opened successfully!"
        << std::endl;


    // ==========================================
    // 3. FPS 相关变量
    // ==========================================

    auto previous_time =
        std::chrono::steady_clock::now();

    double fps = 0.0;


    // ==========================================
    // 4. 主循环
    // ==========================================

    while (true)
    {
        // --------------------------------------
        // 4.1 读取当前帧
        // --------------------------------------

        cv::Mat frame =
            reader.read();

        if (frame.empty())
        {
            std::cout
                << "Video finished."
                << std::endl;

            break;
        }


        // --------------------------------------
        // 4.2 计算 FPS
        // --------------------------------------

        auto current_time =
            std::chrono::steady_clock::now();

        double elapsed =
            std::chrono::duration<double>(
                current_time - previous_time
            ).count();

        previous_time =
            current_time;

        if (elapsed > 0.0)
        {
            fps =
                1.0 / elapsed;
        }


        // --------------------------------------
        // 4.3 装甲板检测
        // --------------------------------------

        cv::Mat binary =
            detector.preprocess(
                frame
            );

        auto contours =
            detector.findContours(
                binary
            );

        auto rects =
            detector.getRotatedRects(
                contours
            );

        auto light_bars =
            detector.filterLightBars(
                rects
            );

        auto armors =
            detector.matchArmors(
                light_bars
            );


        // --------------------------------------
        // 4.4 目标选择
        // --------------------------------------

        TargetResult target =
            selector.select(
                armors,
                frame.size()
            );


        // --------------------------------------
        // 4.5 位姿解算
        // --------------------------------------

        PoseResult pose;

        bool pose_valid = false;

        if (target.valid)
        {
            pose =
                pose_solver.solve(
                    target.armor,
                    frame.size()
                );

            if (pose.success)
            {
                pose_valid = true;
            }
        }


        // --------------------------------------
        // 4.6 调试显示
        // --------------------------------------

        visualizer.show(
            frame,
            binary,
            rects,
            light_bars,
            armors,
            target,
            pose,
            pose_valid,
            fps
        );


        // --------------------------------------
        // 4.7 ESC 退出
        // --------------------------------------

        int key =
            cv::waitKey(30);

        if (key == 27)
        {
            break;
        }
    }


    // ==========================================
    // 5. 释放资源
    // ==========================================

    visualizer.releaseVideoWriter();

    cv::destroyAllWindows();

    return 0;
}