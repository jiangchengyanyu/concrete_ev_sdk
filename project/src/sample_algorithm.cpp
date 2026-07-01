#include "sample_algorithm.h"

#include <algorithm>
#include <glog/logging.h>
#include <sstream>

#define SDKLOG(b) LOG(b) << "[SDKLOG] "

SampleAlgorithm::SampleAlgorithm() {}

SampleAlgorithm::~SampleAlgorithm() {
    UnInit();
}

std::string SampleAlgorithm::ClassName(int label) const {
    if (label == 0) return "excellent";
    if (label == 1) return "good";
    return "unknown";
}

JiErrorCode SampleAlgorithm::Init() {
    m_detector = std::make_shared<SampleDetector>();
    if (!m_detector->Init()) {
        SDKLOG(ERROR) << "Concrete detector init failed";
        return JISDK_RET_FAILED;
    }
    SDKLOG(INFO) << "Concrete SampleAlgorithm init OK";
    return JISDK_RET_SUCCEED;
}

JiErrorCode SampleAlgorithm::UnInit() {
    if (m_detector) {
        m_detector->UnInit();
        m_detector.reset();
    }
    return JISDK_RET_SUCCEED;
}

JiErrorCode SampleAlgorithm::UpdateConfig(const char *args) {
    SDKLOG(INFO) << "UpdateConfig ignored in minimal concrete version: "
                 << (args ? args : "");
    return JISDK_RET_SUCCEED;
}

JiErrorCode SampleAlgorithm::GetOutFrame(JiImageInfo **out, unsigned int &out_count) {
    out_count = m_out_count;

    m_out_image[0].nWidth = m_output_frame.cols;
    m_out_image[0].nHeight = m_output_frame.rows;
    m_out_image[0].nFormat = JI_IMAGE_TYPE_BGR;
    m_out_image[0].nDataType = JI_UNSIGNED_CHAR;
    m_out_image[0].nWidthStride = m_output_frame.step[0];
    m_out_image[0].nHeightStride = m_output_frame.rows;
    m_out_image[0].nDataLen = m_output_frame.total() * m_output_frame.elemSize();
    m_out_image[0].pData = m_output_frame.data;

    *out = m_out_image;
    return JISDK_RET_SUCCEED;
}

JiErrorCode SampleAlgorithm::Process(const cv::Mat &in_frame, const char *args, JiEvent &event) {
    if (in_frame.empty()) {
        event.code = JISDK_CODE_FAILED;
        event.json = "{}";
        return JISDK_RET_FAILED;
    }

    std::vector<BoxInfo> dets;
    if (!m_detector->ProcessImage(in_frame, dets)) {
        event.code = JISDK_CODE_FAILED;
        event.json = "{}";
        return JISDK_RET_FAILED;
    }

    in_frame.copyTo(m_output_frame);

    for (const auto &d : dets) {
        cv::Scalar color = (d.label == 0) ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
        cv::Rect rect(
            static_cast<int>(d.x1),
            static_cast<int>(d.y1),
            static_cast<int>(d.x2 - d.x1),
            static_cast<int>(d.y2 - d.y1)
        );

        cv::rectangle(m_output_frame, rect, color, 5);

        std::ostringstream label;
        label.precision(2);
        label << std::fixed << ClassName(d.label) << " " << d.score;

        cv::putText(
            m_output_frame,
            label.str(),
            cv::Point(static_cast<int>(d.x1), std::max(30, static_cast<int>(d.y1) - 8)),
            cv::FONT_HERSHEY_SIMPLEX,
            1.0,
            color,
            3
        );
    }

    bool is_alert = !dets.empty();

    std::ostringstream json;
    json << "{";
    json << "\"is_alert\":" << (is_alert ? "true" : "false") << ",";
    json << "\"algorithm_data\":{";
    json << "\"is_alert\":" << (is_alert ? "true" : "false") << ",";
    json << "\"target_info\":[";
    for (size_t i = 0; i < dets.size(); ++i) {
        const auto &d = dets[i];
        if (i > 0) json << ",";
        json << "{";
        json << "\"x\":" << static_cast<int>(d.x1) << ",";
        json << "\"y\":" << static_cast<int>(d.y1) << ",";
        json << "\"width\":" << static_cast<int>(d.x2 - d.x1) << ",";
        json << "\"height\":" << static_cast<int>(d.y2 - d.y1) << ",";
        json << "\"name\":\"" << ClassName(d.label) << "\",";
        json << "\"confidence\":" << d.score;
        json << "}";
    }
    json << "]";
    json << "},";
    json << "\"model_data\":{";
    json << "\"objects\":[";
    for (size_t i = 0; i < dets.size(); ++i) {
        const auto &d = dets[i];
        if (i > 0) json << ",";
        json << "{";
        json << "\"x\":" << static_cast<int>(d.x1) << ",";
        json << "\"y\":" << static_cast<int>(d.y1) << ",";
        json << "\"width\":" << static_cast<int>(d.x2 - d.x1) << ",";
        json << "\"height\":" << static_cast<int>(d.y2 - d.y1) << ",";
        json << "\"name\":\"" << ClassName(d.label) << "\",";
        json << "\"confidence\":" << d.score;
        json << "}";
    }
    json << "]";
    json << "}";
    json << "}";

    m_str_out_json = json.str();

    event.code = is_alert ? JISDK_CODE_ALARM : JISDK_CODE_NORMAL;
    event.json = m_str_out_json.c_str();

    SDKLOG(INFO) << "Concrete Process done, dets=" << dets.size()
                 << ", json=" << m_str_out_json;

    return JISDK_RET_SUCCEED;
}
