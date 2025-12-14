#pragma once

#include "RTG.hpp"
#include "Helpers.hpp"
#include "VK.hpp"

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
            ~Workspace();

            void initialize_buffer_pair(VkDeviceSize buffer_size, VkDescriptorSetLayout descriptor_set_layout);
            void update_buffer_pair(uint32_t index);
        };

        WorkspaceManager(RTG &rtg, VkCommandPool command_pool, VkDescriptorPool descriptor_pool, size_t size) : rtg(rtg), command_pool(command_pool), descriptor_pool(descriptor_pool) {
            for(size_t i = 0; i < size; ++i){
                workspaces.push_back(Workspace(*this));
            }
        }

        ~WorkspaceManager();

        std::vector<Workspace> workspaces;
    
    private:
        RTG &rtg; //for buffer creation.
        VkCommandPool command_pool = VK_NULL_HANDLE; //for command buffers; reset at the start of every render.
        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE; //for descriptor sets; reset at the start of every render.
};