#include "sample_detector.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <glog/logging.h>
#include <sys/stat.h>

#define SDKLOG(b) LOG(b) << "[SDKLOG] "

SampleDetector::SampleDetector() {}

SampleDetector::~SampleDetector() {
    UnInit();
}

std::string SampleDetector::FindModelPath() {
    const char *env_path = std::getenv("CONCRETE_MODEL_PATH");
    if (env_path && std::strlen(env_path) > 0) {
        return std::string(env_path);
    }

    std::vector<std::string> candidates = {
        "/usr/local/ev_sdk/model/concrete/best.om",
        "/home/HwHiAiUser/evsdk_concrete_work/project/ev_sdk_demo4.0_evdeploy/model/concrete/best.om",
        "model/concrete/best.om"
    };

    struct stat st {};
    for (const auto &p : candidates) {
        if (stat(p.c_str(), &st) == 0) {
            return p;
        }
    }
    return candidates[1];
}

bool SampleDetector::Init() {
    if (initialized_) {
        return true;
    }

    std::string model_path = FindModelPath();
    SDKLOG(INFO) << "Concrete detector model path: " << model_path;

    aclError ret = aclInit(nullptr);
    if (ret != ACL_SUCCESS) {
        SDKLOG(ERROR) << "aclInit failed, ret=" << ret;
        return false;
    }

    ret = aclrtSetDevice(device_id_);
    if (ret != ACL_SUCCESS) {
        SDKLOG(ERROR) << "aclrtSetDevice failed, ret=" << ret;
        return false;
    }

    ret = aclrtCreateContext(&context_, device_id_);
    if (ret != ACL_SUCCESS) {
        SDKLOG(ERROR) << "aclrtCreateContext failed, ret=" << ret;
        return false;
    }

    ret = aclmdlLoadFromFile(model_path.c_str(), &model_id_);
    if (ret != ACL_SUCCESS) {
        SDKLOG(ERROR) << "aclmdlLoadFromFile failed, ret=" << ret;
        return false;
    }

    model_desc_ = aclmdlCreateDesc();
    ret = aclmdlGetDesc(model_desc_, model_id_);
    if (ret != ACL_SUCCESS) {
        SDKLOG(ERROR) << "aclmdlGetDesc failed, ret=" << ret;
        return false;
    }

    input_size_ = aclmdlGetInputSizeByIndex(model_desc_, 0);
    output_size_ = aclmdlGetOutputSizeByIndex(model_desc_, 0);

    SDKLOG(INFO) << "model input size=" << input_size_ << ", output size=" << output_size_;

    ret = aclrtMalloc(&input_buffer_, input_size_, ACL_MEM_MALLOC_HUGE_FIRST);
    if (ret != ACL_SUCCESS) {
        SDKLOG(ERROR) << "aclrtMalloc input failed, ret=" << ret;
        return false;
    }

    ret = aclrtMalloc(&output_buffer_, output_size_, ACL_MEM_MALLOC_HUGE_FIRST);
    if (ret != ACL_SUCCESS) {
        SDKLOG(ERROR) << "aclrtMalloc output failed, ret=" << ret;
        return false;
    }

    input_dataset_ = aclmdlCreateDataset();
    output_dataset_ = aclmdlCreateDataset();

    aclDataBuffer *input_data = aclCreateDataBuffer(input_buffer_, input_size_);
    aclDataBuffer *output_data = aclCreateDataBuffer(output_buffer_, output_size_);

    ret = aclmdlAddDatasetBuffer(input_dataset_, input_data);
    if (ret != ACL_SUCCESS) {
        SDKLOG(ERROR) << "aclmdlAddDatasetBuffer input failed, ret=" << ret;
        return false;
    }

    ret = aclmdlAddDatasetBuffer(output_dataset_, output_data);
    if (ret != ACL_SUCCESS) {
        SDKLOG(ERROR) << "aclmdlAddDatasetBuffer output failed, ret=" << ret;
        return false;
    }

    initialized_ = true;
    SDKLOG(INFO) << "Concrete detector init OK";
    return true;
}

bool SampleDetector::UnInit() {
    if (!initialized_) {
        return true;
    }

    if (input_dataset_) {
        aclDataBuffer *buf = aclmdlGetDatasetBuffer(input_dataset_, 0);
        if (buf) aclDestroyDataBuffer(buf);
        aclmdlDestroyDataset(input_dataset_);
        input_dataset_ = nullptr;
    }

    if (output_dataset_) {
        aclDataBuffer *buf = aclmdlGetDatasetBuffer(output_dataset_, 0);
        if (buf) aclDestroyDataBuffer(buf);
        aclmdlDestroyDataset(output_dataset_);
        output_dataset_ = nullptr;
    }

    if (input_buffer_) {
        aclrtFree(input_buffer_);
        input_buffer_ = nullptr;
    }

    if (output_buffer_) {
        aclrtFree(output_buffer_);
        output_buffer_ = nullptr;
    }

    if (model_desc_) {
        aclmdlDestroyDesc(model_desc_);
        model_desc_ = nullptr;
    }

    if (model_id_ != 0) {
        aclmdlUnload(model_id_);
        model_id_ = 0;
    }

    if (context_) {
        aclrtDestroyContext(context_);
        context_ = nullptr;
    }

    aclrtResetDevice(device_id_);
    aclFinalize();

    initialized_ = false;
    return true;
}

bool SampleDetector::Preprocess(const cv::Mat &img,
                                std::vector<uint16_t> &input,
                                LetterboxInfo &info) {
    if (img.empty()) {
        return false;
    }

    info.orig_h = img.rows;
    info.orig_w = img.cols;

    float r = std::min(static_cast<float>(input_h_) / img.rows,
                       static_cast<float>(input_w_) / img.cols);

    int new_w = static_cast<int>(std::round(img.cols * r));
    int new_h = static_cast<int>(std::round(img.rows * r));

    cv::Mat resized;
    cv::resize(img, resized, cv::Size(new_w, new_h), 0, 0, cv::INTER_LINEAR);

    int dw = input_w_ - new_w;
    int dh = input_h_ - new_h;

    int left = dw / 2;
    int right = dw - left;
    int top = dh / 2;
    int bottom = dh - top;

    cv::Mat padded;
    cv::copyMakeBorder(resized, padded, top, bottom, left, right,
                       cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114));

    cv::Mat rgb;
    cv::cvtColor(padded, rgb, cv::COLOR_BGR2RGB);

    input.resize(1 * 3 * input_h_ * input_w_);

    const int area = input_h_ * input_w_;
    for (int y = 0; y < input_h_; ++y) {
        const cv::Vec3b *row = rgb.ptr<cv::Vec3b>(y);
        for (int x = 0; x < input_w_; ++x) {
            int idx = y * input_w_ + x;
            input[0 * area + idx] = Float32ToFloat16(row[x][0] / 255.0f);
            input[1 * area + idx] = Float32ToFloat16(row[x][1] / 255.0f);
            input[2 * area + idx] = Float32ToFloat16(row[x][2] / 255.0f);
        }
    }

    info.ratio = r;
    info.dw = left;
    info.dh = top;
    return true;
}

bool SampleDetector::ProcessImage(const cv::Mat &img, std::vector<BoxInfo> &det_results) {
    det_results.clear();

    if (!initialized_ && !Init()) {
        return false;
    }

    std::vector<uint16_t> input;
    LetterboxInfo info;
    if (!Preprocess(img, input, info)) {
        return false;
    }

    if (input.size() * sizeof(uint16_t) != input_size_) {
        SDKLOG(ERROR) << "input size mismatch, got=" << input.size() * sizeof(uint16_t)
                      << ", need=" << input_size_;
        return false;
    }

    aclError ret = aclrtMemcpy(input_buffer_, input_size_,
                               input.data(), input_size_,
                               ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != ACL_SUCCESS) {
        SDKLOG(ERROR) << "aclrtMemcpy H2D failed, ret=" << ret;
        return false;
    }

    ret = aclmdlExecute(model_id_, input_dataset_, output_dataset_);
    if (ret != ACL_SUCCESS) {
        SDKLOG(ERROR) << "aclmdlExecute failed, ret=" << ret;
        return false;
    }

    std::vector<uint16_t> output(output_size_ / sizeof(uint16_t));
    ret = aclrtMemcpy(output.data(), output_size_,
                      output_buffer_, output_size_,
                      ACL_MEMCPY_DEVICE_TO_HOST);
    if (ret != ACL_SUCCESS) {
        SDKLOG(ERROR) << "aclrtMemcpy D2H failed, ret=" << ret;
        return false;
    }

    return Postprocess(output, info, det_results);
}

bool SampleDetector::Postprocess(const std::vector<uint16_t> &raw_output,
                                 const LetterboxInfo &info,
                                 std::vector<BoxInfo> &results) {
    results.clear();

    if (raw_output.size() < static_cast<size_t>(6 * num_boxes_)) {
        SDKLOG(ERROR) << "output size too small: " << raw_output.size();
        return false;
    }

    std::vector<BoxInfo> candidates;
    candidates.reserve(64);

    for (int i = 0; i < num_boxes_; ++i) {
        float xc = Float16ToFloat32(raw_output[0 * num_boxes_ + i]);
        float yc = Float16ToFloat32(raw_output[1 * num_boxes_ + i]);
        float bw = Float16ToFloat32(raw_output[2 * num_boxes_ + i]);
        float bh = Float16ToFloat32(raw_output[3 * num_boxes_ + i]);

        float s0 = Float16ToFloat32(raw_output[4 * num_boxes_ + i]);
        float s1 = Float16ToFloat32(raw_output[5 * num_boxes_ + i]);

        int cls = (s0 >= s1) ? 0 : 1;
        float score = std::max(s0, s1);

        if (score < conf_thresh_) {
            continue;
        }

        float x1 = xc - bw / 2.0f;
        float y1 = yc - bh / 2.0f;
        float x2 = xc + bw / 2.0f;
        float y2 = yc + bh / 2.0f;

        x1 = (x1 - info.dw) / info.ratio;
        x2 = (x2 - info.dw) / info.ratio;
        y1 = (y1 - info.dh) / info.ratio;
        y2 = (y2 - info.dh) / info.ratio;

        x1 = std::max(0.0f, std::min(x1, static_cast<float>(info.orig_w - 1)));
        x2 = std::max(0.0f, std::min(x2, static_cast<float>(info.orig_w - 1)));
        y1 = std::max(0.0f, std::min(y1, static_cast<float>(info.orig_h - 1)));
        y2 = std::max(0.0f, std::min(y2, static_cast<float>(info.orig_h - 1)));

        if (x2 <= x1 || y2 <= y1) {
            continue;
        }

        candidates.push_back(BoxInfo{x1, y1, x2, y2, score, cls});
    }

    std::vector<int> keep = NMS(candidates, nms_thresh_);
    for (int idx : keep) {
        results.push_back(candidates[idx]);
    }

    SDKLOG(INFO) << "Concrete detector candidates=" << candidates.size()
                 << ", after_nms=" << results.size();

    return true;
}

std::vector<int> SampleDetector::NMS(const std::vector<BoxInfo> &boxes, float iou_thresh) {
    std::vector<int> order(boxes.size());
    for (size_t i = 0; i < boxes.size(); ++i) {
        order[i] = static_cast<int>(i);
    }

    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return boxes[a].score > boxes[b].score;
    });

    std::vector<int> keep;
    std::vector<bool> removed(boxes.size(), false);

    for (size_t _i = 0; _i < order.size(); ++_i) {
        int i = order[_i];
        if (removed[i]) continue;

        keep.push_back(i);

        for (size_t _j = _i + 1; _j < order.size(); ++_j) {
            int j = order[_j];
            if (removed[j]) continue;
            if (IoU(boxes[i], boxes[j]) > iou_thresh) {
                removed[j] = true;
            }
        }
    }

    return keep;
}

float SampleDetector::IoU(const BoxInfo &a, const BoxInfo &b) {
    float xx1 = std::max(a.x1, b.x1);
    float yy1 = std::max(a.y1, b.y1);
    float xx2 = std::min(a.x2, b.x2);
    float yy2 = std::min(a.y2, b.y2);

    float w = std::max(0.0f, xx2 - xx1);
    float h = std::max(0.0f, yy2 - yy1);
    float inter = w * h;

    float area_a = std::max(0.0f, a.x2 - a.x1) * std::max(0.0f, a.y2 - a.y1);
    float area_b = std::max(0.0f, b.x2 - b.x1) * std::max(0.0f, b.y2 - b.y1);

    return inter / (area_a + area_b - inter + 1e-6f);
}

uint16_t SampleDetector::Float32ToFloat16(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));

    uint32_t sign = (bits >> 16) & 0x8000;
    int32_t exp = ((bits >> 23) & 0xff) - 127 + 15;
    uint32_t mant = bits & 0x7fffff;

    if (exp <= 0) {
        if (exp < -10) {
            return static_cast<uint16_t>(sign);
        }
        mant = (mant | 0x800000) >> (1 - exp);
        return static_cast<uint16_t>(sign | ((mant + 0x1000) >> 13));
    } else if (exp >= 31) {
        return static_cast<uint16_t>(sign | 0x7c00);
    }

    return static_cast<uint16_t>(sign | (exp << 10) | ((mant + 0x1000) >> 13));
}

float SampleDetector::Float16ToFloat32(uint16_t h) {
    uint32_t sign = (h & 0x8000) << 16;
    uint32_t exp = (h & 0x7c00) >> 10;
    uint32_t mant = h & 0x03ff;

    uint32_t bits;
    if (exp == 0) {
        if (mant == 0) {
            bits = sign;
        } else {
            exp = 1;
            while ((mant & 0x0400) == 0) {
                mant <<= 1;
                exp--;
            }
            mant &= 0x03ff;
            exp = exp + (127 - 15);
            bits = sign | (exp << 23) | (mant << 13);
        }
    } else if (exp == 31) {
        bits = sign | 0x7f800000 | (mant << 13);
    } else {
        exp = exp + (127 - 15);
        bits = sign | (exp << 23) | (mant << 13);
    }

    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
