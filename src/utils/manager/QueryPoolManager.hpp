#pragma once

#include "RTG.hpp"
#include "VK.hpp"

class QueryPoolManager {
    public:
        QueryPoolManager() = default;
        ~QueryPoolManager() = default;

        void create(RTG &rtg, uint32_t workspace_count);
        void destroy(RTG &rtg);

        bool is_enabled() const { return timing_enabled; }

        void begin_frame(VkCommandBuffer command_buffer, uint32_t workspace_index);
        void end_frame(VkCommandBuffer command_buffer, uint32_t workspace_index);

        bool fetch_frame_ms(RTG &rtg, uint32_t workspace_index, double &out_ms) const;

    private:
        VkQueryPool query_pool = VK_NULL_HANDLE;
        float timestamp_period = 0.0f;
        bool timing_enabled = false;
        uint32_t workspace_count = 0;
};
