#pragma once

#include "RTG.hpp"
#include "Helpers.hpp"
#include "VK.hpp"
#include "Pipeline.hpp"

#include <iostream>
#include <string>
#include <cstring>

class WorkspaceManager {
    private:
        VkCommandPool command_pool = VK_NULL_HANDLE; //for command buffers; reset at the start of every render.
        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE; //for descriptor sets; reset at the start of every render.

    public:
        //workspaces hold per-render resources:
        struct Workspace {
            struct BufferPair{
                Helpers::AllocatedBuffer host; //host coherent; mapped
                Helpers::AllocatedBuffer device; //device-local

                BufferPair() = default;
                ~BufferPair();
                BufferPair(BufferPair&& other) noexcept;
                BufferPair& operator=(BufferPair&& other) noexcept;
            };

            struct DescriptorSetGroup {
                VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
                std::vector<std::shared_ptr<Workspace::BufferPair>> buffer_pairs;

                DescriptorSetGroup() = default;
                ~DescriptorSetGroup();
                DescriptorSetGroup(DescriptorSetGroup&& other) noexcept;
                DescriptorSetGroup& operator=(DescriptorSetGroup&& other) noexcept;
            };

            VkCommandBuffer command_buffer = VK_NULL_HANDLE; //from the command pool above; reset at the start of every render.
            WorkspaceManager *manager = nullptr;
            std::vector<std::vector<DescriptorSetGroup>> pipeline_descriptor_set_groups; // [pipelines_index][descriptor_set_index]
            std::unordered_map<std::string, std::shared_ptr<BufferPair>> global_buffer_pairs; // buffer pairs not tied to any descriptor set
            std::vector<std::vector<std::unique_ptr<BufferPair>>> data_buffer_pairs; // [pipelines_index][data_buffer_index] buffer pairs for data buffers that are written to by the CPU and read from shaders, but not tied to any particular descriptor set (e.g., for copying vertex data to the GPU)

            void create(RTG& rtg);
            void destroy(RTG& rtg);

            void write_buffer(
                RTG& rtg, 
                uint32_t pipeline_index, 
                uint32_t descriptor_set_index, 
                uint32_t descriptor_index, 
                void* data, 
                VkDeviceSize size
            );
            void write_global_buffer(
                RTG& rtg, 
                std::string buffer_name, 
                void* data, 
                VkDeviceSize size
            );
            void write_data_buffer(
                RTG& rtg, 
                uint32_t pipeline_index, 
                uint32_t data_buffer_index,
                void* data, 
                VkDeviceSize size
            );
            void update_descriptor(
                RTG& rtg, 
                uint32_t pipeline_index, 
                uint32_t descriptor_set_index, 
                uint32_t descriptor_index, 
                VkDeviceSize size
            );
            void update_global_descriptor(
                RTG& rtg, 
                uint32_t pipeline_index, 
                uint32_t descriptor_set_index, 
                uint32_t descriptor_index, 
                std::string buffer_name, 
                VkDeviceSize size
            );
            void update_data_buffer_pair(
                RTG& rtg, 
                uint32_t pipeline_index, 
                uint32_t data_buffer_index,
                VkDeviceSize size
            );
            void begin_recording();
            void end_recording();
            void reset_recording();

            Workspace(WorkspaceManager &manager) : manager(&manager) {}
            ~Workspace();
            Workspace(Workspace&& other) noexcept;
            Workspace& operator=(Workspace&& other) noexcept;
        };

        struct GlobalBufferConfig {
            std::string name;
            VkDeviceSize size;
            VkBufferUsageFlagBits usage;
        };

        WorkspaceManager() = default;
        ~WorkspaceManager();

        void create(
            RTG& rtg, 
            const std::vector<std::vector<Pipeline::BlockDescriptorConfig>> &&block_descriptor_configs_by_pipeline, 
            const std::vector<GlobalBufferConfig> &&global_buffer_configs, 
            const std::vector<size_t> &&global_buffer_counts,
            uint32_t num_workspaces
        );  
        void destroy(RTG& rtg);

        void write_all_buffers(
            RTG& rtg, 
            uint32_t pipeline_index, 
            uint32_t descriptor_set_index, 
            uint32_t descriptor_index, 
            void* data, 
            VkDeviceSize size
        );
        void write_all_global_buffers(
            RTG& rtg, 
            std::string buffer_name, 
            void* data, 
            VkDeviceSize size
        );
        void update_all_descriptors(
            RTG& rtg, 
            uint32_t pipeline_index, 
            uint32_t descriptor_set_index, 
            uint32_t descriptor_index, 
            VkDeviceSize size
        );
        void update_all_global_descriptors(
            RTG& rtg, 
            uint32_t pipeline_index, 
            uint32_t descriptor_set_index, 
            uint32_t descriptor_index, 
            std::string buffer_name, 
            VkDeviceSize size
        );
        
        std::vector<Workspace> workspaces;
        std::vector<std::vector<Pipeline::BlockDescriptorConfig>> block_descriptor_configs_by_pipeline;
        std::vector<GlobalBufferConfig> global_buffer_configs;
        std::vector<size_t> global_buffer_counts;

        static const std::unordered_map<VkDescriptorType, VkBufferUsageFlagBits> descriptor_type_to_buffer_usage;
};