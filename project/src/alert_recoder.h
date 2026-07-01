/**
 1、由于算法存在漏检的情况，所以不能直接用每一帧都检测到目标行为来进行计时，这样会因为漏检
而导致计时中断。可以新增以下逻辑，开发者可参考，按需修改：
      1）当算法在过去一段较短时间内，检测到目标行为的帧数达到一定比例，则认为这段时间内一直存
在此目标行为。
         例如，在过去2秒内，检测到目标的帧数达到80%时，则认为过去2秒内一直存在此目标。
      2）而这个时间段和帧数比例的阈值作为配置参数进行设置，时间段使用float类型的配置参数 
alert_time_thresh，帧数比例阈值使用float类型的配置参数 alert_frame_ratio 。
      3）算法内部需要用一个变量 alert_starting_time ，来记录本次持续目标行为开始的时刻。并且用
一个变量 time_elapsed ，来记录一直发生目标行为的时长。当算法在过去的 alert_time_thresh 时段
内，检测到的目标行为的帧数达到 alert_frame_ratio 时，算法认为这段时间内一直存在此目标行为，并
一直根据当前时刻current_time与alert_starting_time的差，来增大 time_elapsed 的时长。
      4）当time_elapsed超过一个预设的时间长度时，算法进行报警。这个预设的时长作为一个配置参
数，名为 max_alert_wait_time ，float类型，单位为秒。注意， max_alert_wait_time 必须是一个比 
alert_time_thresh 更长的时间数值。
 * **/
#ifndef JI_ALERT_RECODER
#define JI_ALERT_RECODER

#include "configuration.hpp"
#include <queue>

class EVAlertRecorder
{
  public:
    using Time = std::chrono::steady_clock::time_point;
    using Record = std::pair<bool, Time>;
    EVAlertRecorder(const RecoderConfig &cfg) : m_config(cfg) {}
    ~EVAlertRecorder() = default;
    /**
     * @param current_ret 当前帧是否有告警目标 true=有 false=无
     * @param current_time 当前时间节点
     * @return std::pair<bool, float> 返回结果<是否满足告警需求，连续检测到告警目标的持续时间(s)>
     */
    std::pair<bool, float> push(bool current_ret, Time current_time);

  private:
    RecoderConfig m_config{};
    std::queue<Record> m_records{};
    Time m_start_time{};   // 告警计数起始时间
    size_t m_num_true{0};  // 队列中检测到有告警目标的帧数量
    size_t m_num_false{0}; // 队列中检测到无告警目标的帧数量
};
#endif