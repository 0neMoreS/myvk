#pragma once

#include "RTG.hpp"
#include "Helpers.hpp"
#include "VK.hpp"
#include "Pipeline.hpp"

#include <iostream>

class WorkspaceManager {
    private:
        VkCommandPool command_pool = VK_NULL_HANDLE; //for command buffers; reset at the start of every render.
        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE; //for descriptor sets; reset at the start of every render.

    public:
        //workspaces hold per-render resources:
        struct Workspace {
            VkCommandBuffer command_buffer = VK_NULL_HANDLE; //from the command pool above; reset at the start of every render.

            struct BufferPair{
                Helpers::AllocatedBuffer host; //host coherent; mapped
                Helpers::AllocatedBuffer device; //device-local
                VkDescriptorSet descriptor; //references World

                BufferPair() = default;
                ~BufferPair() = default;
                BufferPair(BufferPair&& other) noexcept;
                BufferPair& operator=(BufferPair&& other) noexcept;
            };

            std::vector<BufferPair> buffer_pairs;
            WorkspaceManager *manager = nullptr;

            void create(RTG& rtg, std::vector<Pipeline::BlockDescriptorConfig> &pipeline_configs);
            void destroy(RTG& rtg);

            void copy_buffer(RTG& rtg, std::vector<Pipeline::BlockDescriptorConfig> &pipeline_configs, uint32_t index, VkDeviceSize size);
            void update_descriptor(RTG& rtg, std::vector<Pipeline::BlockDescriptorConfig> &pipeline_configs, uint32_t index, VkDeviceSize size);
            void begin_recording();
            void end_recording();
            void reset_recoring();

            Workspace(WorkspaceManager &manager) : manager(&manager) {}
            ~Workspace() = default;
            Workspace(Workspace&& other) noexcept;
            Workspace& operator=(Workspace&& other) noexcept;
        };

        WorkspaceManager() = default;
        ~WorkspaceManager();

        void create(RTG& rtg, std::vector<Pipeline::BlockDescriptorConfig> &pipeline_configs, uint32_t num_workspaces);  
        void destroy(RTG& rtg);

        void copy_all_buffers(RTG& rtg, std::vector<Pipeline::BlockDescriptorConfig> &pipeline_configs, uint32_t index, VkDeviceSize size);
        void update_all_descriptors(RTG& rtg, std::vector<Pipeline::BlockDescriptorConfig> &pipeline_configs, uint32_t index, VkDeviceSize size);
        std::vector<Workspace> workspaces;

        static const std::unordered_map<VkDescriptorType, VkBufferUsageFlagBits> descriptor_type_to_buffer_usage;
};