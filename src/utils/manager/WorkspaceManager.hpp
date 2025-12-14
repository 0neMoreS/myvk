#pragma once

#include "RTG.hpp"
#include "Helpers.hpp"
#include "VK.hpp"
#include "Pipeline.hpp"

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

                // Default constructor
                BufferPair() = default;

                // Move constructor
                BufferPair(BufferPair&& other) noexcept
                    : host(std::move(other.host)),
                    device(std::move(other.device)),
                    descriptor(other.descriptor) {
                    other.descriptor = VK_NULL_HANDLE;
                }

                // Move assignment
                BufferPair& operator=(BufferPair&& other) noexcept {
                    if (this != &other) {
                        host = std::move(other.host);
                        device = std::move(other.device);
                        descriptor = other.descriptor;
                        other.descriptor = VK_NULL_HANDLE;
                    }
                    return *this;
                }
            };

            std::vector<BufferPair> buffer_pairs;
            WorkspaceManager &manager;

            void create(RTG& rtg, std::vector<DescriptorConfig> &pipeline_configs);
            void destroy(RTG& rtg);

            void copy_buffer(RTG& rtg, std::vector<DescriptorConfig> &pipeline_configs, uint32_t index, VkDeviceSize size);
            void update_descriptor(RTG& rtg, std::vector<DescriptorConfig> &pipeline_configs, uint32_t index, VkDeviceSize size);

            Workspace(WorkspaceManager &manager) : manager(manager) {}
            ~Workspace() = default;

            // movable (constructor only; assignment deleted due to reference member)
            Workspace(Workspace&& other) noexcept
                : command_buffer(other.command_buffer),
                  buffer_pairs(std::move(other.buffer_pairs)),
                  manager(other.manager) {
                other.command_buffer = VK_NULL_HANDLE;
            }
            Workspace& operator=(Workspace&& other) noexcept = delete;
        };

        WorkspaceManager() = default;
        ~WorkspaceManager() = default;

        void create(RTG& rtg, std::vector<DescriptorConfig> &pipeline_configs, uint32_t num_workspaces);  
        void destroy(RTG& rtg);

        void copy_all_buffers(RTG& rtg, std::vector<DescriptorConfig> &pipeline_configs, uint32_t index, VkDeviceSize size);
        void update_all_descriptors(RTG& rtg, std::vector<DescriptorConfig> &pipeline_configs, uint32_t index, VkDeviceSize size);

        std::vector<Workspace> workspaces;
};