#ifndef JI_SAMPLEALGORITHM_HPP
#define JI_SAMPLEALGORITHM_HPP

#include <memory>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

#include "ji.h"
#include "sample_detector.h"

class SampleAlgorithm {
public:
    SampleAlgorithm();
    ~SampleAlgorithm();

    JiErrorCode Init();
    JiErrorCode UnInit();
    JiErrorCode Process(const cv::Mat &in_frame, const char *args, JiEvent &event);
    JiErrorCode UpdateConfig(const char *args);
    JiErrorCode GetOutFrame(JiImageInfo **out, unsigned int &out_count);

private:
    std::string ClassName(int label) const;

private:
    cv::Mat m_output_frame;
    JiImageInfo m_out_image[1];
    unsigned int m_out_count = 1;
    std::string m_str_out_json;
    std::shared_ptr<SampleDetector> m_detector{nullptr};
};

#endif
