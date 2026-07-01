#ifndef CONCRETE_SAMPLE_DETECTOR_H
#define CONCRETE_SAMPLE_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <acl/acl.h>

struct BoxInfo {
    float x1;
    float y1;
    float x2;
    float y2;
    float score;
    int label;
};

class SampleDetector {
public:
    SampleDetector();
    ~SampleDetector();

    bool Init();
    bool UnInit();
    bool ProcessImage(const cv::Mat &img, std::vector<BoxInfo> &det_results);

private:
    struct LetterboxInfo {
        float ratio;
        int dw;
        int dh;
        int orig_w;
        int orig_h;
    };

    std::string FindModelPath();
    bool Preprocess(const cv::Mat &img, std::vector<uint16_t> &input, LetterboxInfo &info);
    bool Postprocess(const std::vector<uint16_t> &raw_output,
                     const LetterboxInfo &info,
                     std::vector<BoxInfo> &results);
    std::vector<int> NMS(const std::vector<BoxInfo> &boxes, float iou_thresh);
    static uint16_t Float32ToFloat16(float value);
    static float Float16ToFloat32(uint16_t value);
    static float IoU(const BoxInfo &a, const BoxInfo &b);

private:
    int device_id_{0};
    bool initialized_{false};

    uint32_t model_id_{0};
    aclmdlDesc *model_desc_{nullptr};
    aclrtContext context_{nullptr};

    void *input_buffer_{nullptr};
    void *output_buffer_{nullptr};
    size_t input_size_{0};
    size_t output_size_{0};

    aclmdlDataset *input_dataset_{nullptr};
    aclmdlDataset *output_dataset_{nullptr};

    const int input_w_{640};
    const int input_h_{640};
    const int num_classes_{2};
    const int num_boxes_{8400};

    float conf_thresh_{0.25f};
    float nms_thresh_{0.45f};
};

#endif
