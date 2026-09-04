#ifndef TARGET_SELECTOR_HPP
#define TARGET_SELECTOR_HPP

#include <opencv2/opencv.hpp>
#include <vector>

#include "armor_detector.hpp"
#include "config.hpp"


enum class TargetStatus
{
    NO_TARGET,
    DETECTED,
    TRACKING,
    TEMP_LOST
};


struct TargetResult
{
    bool valid = false;

    Armor armor;

    TargetStatus status = TargetStatus::NO_TARGET;
};


class TargetSelector
{
public:
    TargetSelector(
        const TargetConfig& target_config
    );

    TargetResult select(
        const std::vector<Armor>& armors,
        const cv::Size& image_size
    );

private:
    int detected_frames_;
    int lost_frames_;

    bool had_target_;

    cv::Point2f last_target_center_;

    int tracking_threshold_;
    int temporary_lost_threshold_;
};


#endif