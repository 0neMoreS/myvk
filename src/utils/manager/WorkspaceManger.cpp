#include "WorkspaceManager.hpp"

void WorkspaceManager::Workspace::initialize_buffer_pair(VkDeviceSize buffer_size, VkDescriptorSetLayout descriptor_set_layout) {
    Workspace::BufferPair buffer_pair;

    { //allocate command buffer:
        VkCommandBufferAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool =  manager.command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK( vkAllocateCommandBuffers(manager.rtg.device, &alloc_info, &command_buffer) );
    }

    // allocate data buffers:
    buffer_pair.host = manager.rtg.helpers.create_buffer(
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        Helpers::Mapped
    );
    buffer_pair.device = manager.rtg.helpers.create_buffer(
        buffer_size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped
    );

    { //allocate descriptor set for descriptor
        VkDescriptorSetAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = manager.descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &descriptor_set_layout,
        };

        VK( vkAllocateDescriptorSets(manager.rtg.device, &alloc_info, &buffer_pair.descriptor) );
    }

    this->buffer_pairs.push_back(std::move(buffer_pair));
}

void WorkspaceManager::Workspace::update_buffer_pair(uint32_t index) {
    auto& buffer_pair = buffer_pairs[index];

    { //point descriptor to the buffer:
        VkDescriptorBufferInfo buffer_info{
            .buffer = buffer_pair.device.handle,
            .offset = 0,
            .range = buffer_pair.device.size,
        };

        std::array< VkWriteDescriptorSet, 1 > writes{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = buffer_pair.descriptor,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &buffer_info,
            },
        };

        vkUpdateDescriptorSets(
            manager.rtg.device, //device
            uint32_t(writes.size()), //descriptorWriteCount
            writes.data(), //pDescriptorWrites
            0, //descriptorCopyCount
            nullptr //pDescriptorCopies
        );
    }
}

WorkspaceManager::Workspace::~Workspace() {
    for (auto& buffer_pair : buffer_pairs) {
        if (command_buffer != VK_NULL_HANDLE) {
			vkFreeCommandBuffers(manager.rtg.device, manager.command_pool, 1, &command_buffer);
			command_buffer = VK_NULL_HANDLE;
		}
        if (buffer_pair.host.handle != VK_NULL_HANDLE) {
			manager.rtg.helpers.destroy_buffer(std::move(buffer_pair.host));
		}
        if (buffer_pair.device.handle != VK_NULL_HANDLE) {
			manager.rtg.helpers.destroy_buffer(std::move(buffer_pair.device));
		}
    }
}

WorkspaceManager::~WorkspaceManager() {
    workspaces.clear();

    if(descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(rtg.device, descriptor_pool, nullptr);
        descriptor_pool = VK_NULL_HANDLE;
    }

    if (command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(rtg.device, command_pool, nullptr);
        command_pool = VK_NULL_HANDLE;
    }
}