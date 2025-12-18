#pragma once

#include "RTG.hpp"
#include "Helpers.hpp"
#include "VK.hpp"
#include "Pipeline.hpp"

#include <iostream>
#include <string>

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

            std::vector<std::vector<BufferPair>> pipeline_buffer_pairs; //outer: pipelines, inner: descriptors per pipeline
            WorkspaceManager *manager = nullptr;

            void create(RTG& rtg);
            void destroy(RTG& rtg);

            void copy_buffer(RTG& rtg, uint32_t pipeline_index, uint32_t descriptor_index, VkDeviceSize size);
            void update_descriptor(RTG& rtg, uint32_t pipeline_index, uint32_t descriptor_index, VkDeviceSize size);
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

        void create(RTG& rtg, const std::vector<std::vector<Pipeline::BlockDescriptorConfig>> &&block_descriptor_configs_by_pipeline, uint32_t num_workspaces);  
        void destroy(RTG& rtg);

        void copy_all_buffers(RTG& rtg, uint32_t pipeline_index, uint32_t descriptor_index, VkDeviceSize size);
        void update_all_descriptors(RTG& rtg, uint32_t pipeline_index, uint32_t descriptor_index, VkDeviceSize size);
        
        std::vector<Workspace> workspaces;
        std::vector<std::vector<Pipeline::BlockDescriptorConfig>> block_descriptor_configs_by_pipeline;

        static const std::unordered_map<VkDescriptorType, VkBufferUsageFlagBits> descriptor_type_to_buffer_usage;
};