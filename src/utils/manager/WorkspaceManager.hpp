#pragma once

#include "RTG.hpp"
#include "Helpers.hpp"
#include "VK.hpp"
#include "Pipeline.hpp"

class WorkspaceManager {
    public:
        //workspaces hold per-render resources:
        struct Workspace {
            VkCommandBuffer command_buffer = VK_NULL_HANDLE; //from the command pool above; reset at the start of every render.

            struct BufferPair{
                Helpers::AllocatedBuffer host; //host coherent; mapped
                Helpers::AllocatedBuffer device; //device-local
                VkDescriptorSet descriptor; //references World
            };

            std::vector<BufferPair> buffer_pairs;
            WorkspaceManager &manager;

            Workspace(WorkspaceManager &manager) : manager(manager) {}
            ~Workspace() = default;

            void create(RTG& rtg, std::vector<DescriptorConfig> &pipeline_configs);
            void destroy(RTG& rtg);

            void update_buffer_pair(RTG& rtg, uint32_t index);
        };
        
        WorkspaceManager() = default;
        ~WorkspaceManager() = default;

        void create(RTG& rtg, std::vector<DescriptorConfig> &pipeline_configs, uint32_t num_workspaces);  
        void destroy(RTG& rtg);

        void update_workspace_buffer_pairs(RTG& rtg, uint32_t index);

        std::vector<Workspace> workspaces;
    
    private:
        VkCommandPool command_pool = VK_NULL_HANDLE; //for command buffers; reset at the start of every render.
        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE; //for descriptor sets; reset at the start of every render.
};