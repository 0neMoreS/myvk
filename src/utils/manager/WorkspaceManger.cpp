#include "WorkspaceManager.hpp"

void WorkspaceManager::Workspace::create(RTG& rtg, std::vector<DescriptorConfig> &pipeline_configs) {
    for(auto &config : pipeline_configs) {
        Workspace::BufferPair buffer_pair;

        { //allocate command buffer:
            VkCommandBufferAllocateInfo alloc_info{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool =  manager.command_pool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            VK( vkAllocateCommandBuffers(rtg.device, &alloc_info, &command_buffer) );
        }

        // allocate data buffers:
        buffer_pair.host = rtg.helpers.create_buffer(
            config.size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            Helpers::Mapped
        );
        buffer_pair.device = rtg.helpers.create_buffer(
            config.size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            Helpers::Unmapped
        );

        { //allocate descriptor set for descriptor
            VkDescriptorSetAllocateInfo alloc_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = manager.descriptor_pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &config.set_layout,
            };

            VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &buffer_pair.descriptor) );
        }

        this->buffer_pairs.push_back(std::move(buffer_pair));
    }
}

void WorkspaceManager::Workspace::destroy(RTG &rtg) {
    for (auto& buffer_pair : buffer_pairs) {
        if (command_buffer != VK_NULL_HANDLE) {
			vkFreeCommandBuffers(rtg.device, manager.command_pool, 1, &command_buffer);
			command_buffer = VK_NULL_HANDLE;
		}
        if (buffer_pair.host.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(buffer_pair.host));
		}
        if (buffer_pair.device.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(buffer_pair.device));
		}
    }
}

void WorkspaceManager::Workspace::update_buffer_pair(RTG &rtg, uint32_t index) {
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
            rtg.device, //device
            uint32_t(writes.size()), //descriptorWriteCount
            writes.data(), //pDescriptorWrites
            0, //descriptorCopyCount
            nullptr //pDescriptorCopies
        );
    }
}

void WorkspaceManager::create(RTG &rtg, std::vector<DescriptorConfig> &pipeline_configs, uint32_t num_workspaces) {
    { //create command pool
		VkCommandPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = rtg.graphics_queue_family.value(),
		};
		VK( vkCreateCommandPool(rtg.device, &create_info, nullptr, &command_pool) );
	}

    { //create descriptor pool:
        std::unordered_map<VkDescriptorType, uint32_t> descriptor_map;
            for (auto t : pipeline_configs) descriptor_map[t.type]++;

        uint32_t per_workspace = uint32_t(rtg.workspaces.size()); //for easier-to-read counting

        std::vector<VkDescriptorPoolSize> pool_sizes;
        for (auto [type, count] : descriptor_map) {
            pool_sizes.emplace_back(
                VkDescriptorPoolSize{
                    .type = type,
                    .descriptorCount = count * per_workspace,
                });
        }

        VkDescriptorPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, //because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = 3 * per_workspace, //one set per workspace
			.poolSizeCount = uint32_t(pool_sizes.size()),
			.pPoolSizes = pool_sizes.data(),
		};

		VK( vkCreateDescriptorPool(rtg.device, &create_info, nullptr, &descriptor_pool) );
    }

    for(uint32_t i = 0; i < num_workspaces; i++) {
        workspaces.emplace_back(Workspace{*this});
    }
}

void WorkspaceManager::destroy(RTG &rtg) {
    for(auto &workspace : workspaces) {
        workspace.destroy(rtg);
    }

    if(descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(rtg.device, descriptor_pool, nullptr);
        descriptor_pool = VK_NULL_HANDLE;
    }

    if (command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(rtg.device, command_pool, nullptr);
        command_pool = VK_NULL_HANDLE;
    }
}

void WorkspaceManager::update_workspace_buffer_pairs(RTG& rtg, uint32_t index){
    for (auto& workspace : workspaces) {
        workspace.update_buffer_pair(rtg, index);
    }
}