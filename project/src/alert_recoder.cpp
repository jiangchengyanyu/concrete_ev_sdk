#include "alert_recoder.h"

std::pair<bool, float> EVAlertRecorder::push(bool current_ret, Time current_time)
{
    m_records.push({current_ret, current_time});
    if (current_ret)
    {
        m_num_true++;
    }
    else
    {
        m_num_false++;
    }
    auto time_span =
        std::chrono::duration_cast<std::chrono::milliseconds>(current_time - m_records.front().second).count() /
        1000.0f;
    if (time_span > m_config.alert_time_thresh)
    {
        auto oldest_retult = m_records.front();
        m_records.pop();
        if (oldest_retult.first)
        {
            m_num_true--;
        }
        else
        {
            m_num_false--;
        }
        auto rate = m_num_true * 1.0f / (m_num_false + m_num_true);
        if (current_ret)
        {
            if (m_start_time.time_since_epoch().count() == 0)
            {
                m_start_time = current_time;
            }
            else
            {
                if (rate > m_config.alert_frame_ratio)
                {
                    auto time_elapsed =
                        std::chrono::duration_cast<std::chrono::milliseconds>(current_time - m_start_time).count() /
                        1000.f;
                    if (time_elapsed > m_config.max_alert_wait_time)
                    {
                        return {true, time_elapsed};
                    }
                }
                else
                {
                    m_start_time = current_time;
                }
            }
        }
        else
        {
            if (m_start_time.time_since_epoch().count() != 0)
            {
                if (rate < m_config.alert_frame_ratio)
                {
                    m_start_time = {};
                }
            }
        }
    }
    else
    {
        if (current_ret)
        {
            if (m_start_time.time_since_epoch().count() == 0)
            {
                m_start_time = current_time;
            }
        }
    }
    return {false, 0.0f};
}