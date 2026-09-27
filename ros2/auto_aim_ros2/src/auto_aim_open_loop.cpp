#include <chrono>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/image_encodings.hpp>
#include <sensor_msgs/msg/image.hpp>

#include "armor_detector.hpp"
#include "config.hpp"
#include "pose_solver.hpp"
#include "target_selector.hpp"

class AutoAimOpenLoop : public rclcpp::Node
{
public:
    AutoAimOpenLoop()
        : Node("auto_aim_open_loop"),
          previous_time_(std::chrono::steady_clock::now())
    {
        Config config = createDefaultConfig();

        const std::string config_path =
            ament_index_cpp::get_package_share_directory(
                "auto_aim_ros2") +
            "/config/config.yaml";

        if (!loadConfig(config_path, config))
        {
            RCLCPP_WARN(
                get_logger(),
                "Failed to load config; using defaults.");
        }


        // 仿真相机内参：
        // fx = fy = 1303.675283386667，图像宽度为 1440。
        config.camera.fx_scale =
            1303.675283386667 / 1440.0;
        config.camera.fy_scale =
            1303.675283386667 / 1440.0;

        detector_ = std::make_unique<ArmorDetector>(
            config.enemy,
            config.preprocess,
            config.light_bar,
            config.armor);

        selector_ = std::make_unique<TargetSelector>(
            config.target);

        pose_solver_ = std::make_unique<PoseSolver>(
            config.camera,
            config.armor_size);

        rclcpp::QoS qos(rclcpp::KeepLast(1));
        qos.reliable();
        qos.durability_volatile();

        subscription_ =
            create_subscription<sensor_msgs::msg::Image>(
                "/image_raw",
                qos,
                std::bind(
                    &AutoAimOpenLoop::imageCallback,
                    this,
                    std::placeholders::_1));

        cv::namedWindow(
            "ROS2 Auto Aim Open Loop",
            cv::WINDOW_NORMAL);

        cv::resizeWindow(
            "ROS2 Auto Aim Open Loop",
            960,
            720);

        cv::namedWindow(
            "ROS2 Auto Aim Binary",
            cv::WINDOW_NORMAL);

        cv::resizeWindow(
            "ROS2 Auto Aim Binary",
            960,
            720);

        RCLCPP_INFO(
            get_logger(),
            "Waiting for simulator images on /image_raw...");
    }

    ~AutoAimOpenLoop() override
    {
        cv::destroyAllWindows();
    }

private:
    void imageCallback(
        const sensor_msgs::msg::Image::ConstSharedPtr msg)
    {
        cv::Mat frame;

        try
        {
            frame = cv_bridge::toCvCopy(
                        msg,
                        sensor_msgs::image_encodings::BGR8)
                        ->image;
        }
        catch (const cv_bridge::Exception &error)
        {
            RCLCPP_ERROR(
                get_logger(),
                "cv_bridge conversion failed: %s",
                error.what());
            return;
        }

        if (frame.empty())
        {
            return;
        }

        const auto now =
            std::chrono::steady_clock::now();

        const double elapsed =
            std::chrono::duration<double>(
                now - previous_time_)
                .count();

        previous_time_ = now;

        const double fps =
            elapsed > 0.0 ? 1.0 / elapsed : 0.0;

        cv::Mat binary =
            detector_->preprocess(frame);

        const auto contours =
            detector_->findContours(binary);

        const auto rects =
            detector_->getRotatedRects(contours);

        const auto light_bars =
            detector_->filterLightBars(rects);

        const auto armors =
            detector_->matchArmors(light_bars);

        const TargetResult target =
            selector_->select(
                armors,
                frame.size());

        PoseResult pose;

        if (target.valid)
        {
            pose = pose_solver_->solve(
                target.armor,
                frame.size());
        }

        cv::Mat result = frame.clone();

        // 用绿色矩形显示通过筛选的灯条。
        for (const auto &light : light_bars)
        {
            cv::Point2f points[4];
            light.points(points);

            for (int i = 0; i < 4; ++i)
            {
                cv::line(
                    result,
                    points[i],
                    points[(i + 1) % 4],
                    cv::Scalar(0, 255, 0),
                    2);
            }
        }

        for (const auto &armor : armors)
        {
            cv::line(
                result,
                armor.left_top,
                armor.right_top,
                cv::Scalar(0, 255, 255),
                2);
            cv::line(
                result,
                armor.right_top,
                armor.right_bottom,
                cv::Scalar(0, 255, 255),
                2);
            cv::line(
                result,
                armor.right_bottom,
                armor.left_bottom,
                cv::Scalar(0, 255, 255),
                2);
            cv::line(
                result,
                armor.left_bottom,
                armor.left_top,
                cv::Scalar(0, 255, 255),
                2);
        }

        if (target.valid)
        {
            cv::drawMarker(
                result,
                target.armor.center,
                cv::Scalar(0, 0, 255),
                cv::MARKER_CROSS,
                30,
                2);
        }

        std::ostringstream status_text;
        status_text
            << std::fixed
            << std::setprecision(1)
            << "FPS: " << fps
            << "  Rects: " << rects.size()
            << "  Lights: " << light_bars.size()
            << "  Armors: " << armors.size();

        cv::putText(
            result,
            status_text.str(),
            cv::Point(20, 35),
            cv::FONT_HERSHEY_SIMPLEX,
            0.7,
            cv::Scalar(0, 255, 0),
            2);

        if (pose.success)
        {
            std::ostringstream pose_text;
            pose_text
                << std::fixed
                << std::setprecision(2)
                << "Yaw: " << pose.yaw
                << "  Pitch: " << pose.pitch
                << "  Distance: " << pose.distance << " mm";

            cv::putText(
                result,
                pose_text.str(),
                cv::Point(20, 70),
                cv::FONT_HERSHEY_SIMPLEX,
                0.7,
                cv::Scalar(0, 255, 0),
                2);
        }

        cv::imshow(
            "ROS2 Auto Aim Open Loop",
            result);

        cv::imshow(
            "ROS2 Auto Aim Binary",
            binary);

        const int key = cv::waitKey(1);

        if (key == 27 || key == 'q' || key == 'Q')
        {
            rclcpp::shutdown();
        }
    }

    std::unique_ptr<ArmorDetector> detector_;
    std::unique_ptr<TargetSelector> selector_;
    std::unique_ptr<PoseSolver> pose_solver_;

    rclcpp::Subscription<
        sensor_msgs::msg::Image>::SharedPtr subscription_;

    std::chrono::steady_clock::time_point previous_time_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(
        std::make_shared<AutoAimOpenLoop>());
    rclcpp::shutdown();
    return 0;
}
