#include "WorkspaceManager.hpp"

const std::unordered_map<VkDescriptorType, VkBufferUsageFlagBits> WorkspaceManager::descriptor_type_to_buffer_usage{{
    {VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
    {VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT}
}};

WorkspaceManager::Workspace::BufferPair::BufferPair(BufferPair&& other) noexcept
    : host(std::move(other.host)),
    device(std::move(other.device)),
    descriptor(std::move(other.descriptor)) {
    other.descriptor = VK_NULL_HANDLE;
}

WorkspaceManager::Workspace::BufferPair& WorkspaceManager::Workspace::BufferPair::operator=(BufferPair&& other) noexcept {
    if (this != &other) {
        host = std::move(other.host);
        device = std::move(other.device);
        descriptor = std::move(other.descriptor);
        other.descriptor = VK_NULL_HANDLE;
    }
    return *this;
}

WorkspaceManager::Workspace::Workspace(Workspace&& other) noexcept
    : command_buffer(std::move(other.command_buffer)),
        buffer_pairs(std::move(other.buffer_pairs)),
        manager(std::move(other.manager)) {
    other.command_buffer = VK_NULL_HANDLE;
}

WorkspaceManager::Workspace& WorkspaceManager::Workspace::operator=(Workspace&& other) noexcept {
    if (this != &other) {
        command_buffer = std::move(other.command_buffer);
        buffer_pairs = std::move(other.buffer_pairs);
        manager = std::move(other.manager);
        other.manager = nullptr;
        other.command_buffer = VK_NULL_HANDLE;
    }
    return *this;
};

void WorkspaceManager::Workspace::create(RTG& rtg, std::vector<Pipeline::BlockDescriptorConfig> &pipeline_configs) {
    { // allocate one command buffer per workspace
        VkCommandBufferAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool =  manager->command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK( vkAllocateCommandBuffers(rtg.device, &alloc_info, &command_buffer) );
    }

    // The index in the buffer_pairs vector is the same as the index in the pipeline_configs vector
    for (size_t i = 0; i < pipeline_configs.size(); i++) {
        Workspace::BufferPair buffer_pair;
        buffer_pairs.push_back(std::move(buffer_pair));
    }
}

void WorkspaceManager::Workspace::destroy(RTG &rtg) {
    for (auto& buffer_pair : buffer_pairs) {
        if (command_buffer != VK_NULL_HANDLE) {
			vkFreeCommandBuffers(rtg.device, manager->command_pool, 1, &command_buffer);
			command_buffer = VK_NULL_HANDLE;
		}
        if (buffer_pair.host.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(buffer_pair.host));
		}
        if (buffer_pair.device.handle != VK_NULL_HANDLE) {
			rtg.helpers.destroy_buffer(std::move(buffer_pair.device));
		}
        buffer_pair.descriptor = VK_NULL_HANDLE; // descriptor is freed when the descriptor pool is destroyed, and pool doesn't use this handle to locate descriptors
    }
}

void WorkspaceManager::Workspace::update_descriptor(RTG &rtg, std::vector<Pipeline::BlockDescriptorConfig> &pipeline_configs, uint32_t index, VkDeviceSize size) {
    auto& buffer_pair = buffer_pairs[index];
    auto& config = pipeline_configs[index];

    if(buffer_pair.host.handle != VK_NULL_HANDLE) {
        rtg.helpers.destroy_buffer(std::move(buffer_pair.host));
    }
    if(buffer_pair.device.handle != VK_NULL_HANDLE) {
        rtg.helpers.destroy_buffer(std::move(buffer_pair.device));
    }

    // allocate data buffers:
    buffer_pair.host = rtg.helpers.create_buffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        Helpers::Mapped
    );
    buffer_pair.device = rtg.helpers.create_buffer(
        size,
        WorkspaceManager::descriptor_type_to_buffer_usage.at(config.type) | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped
    );

    { //allocate descriptor set for descriptor
        VkDescriptorSetAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = manager->descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &config.layout,
        };

        VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &buffer_pair.descriptor) );
    }

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
                .descriptorType = config.type,
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

void WorkspaceManager::Workspace::copy_buffer(RTG& rtg, std::vector<Pipeline::BlockDescriptorConfig> &pipeline_configs, uint32_t index, VkDeviceSize size){
    auto &buffer_pair = buffer_pairs[index];

    VkBufferCopy copy_region{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = size,
        };
    vkCmdCopyBuffer(command_buffer, buffer_pair.host.handle, buffer_pair.device.handle, 1, &copy_region);
}

void WorkspaceManager::Workspace::begin_recording(){
    VkCommandBufferBeginInfo begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, //will record again every submit
		};
		
    VK( vkBeginCommandBuffer(command_buffer, &begin_info) );
}

void WorkspaceManager::Workspace::end_recording(){
    VK( vkEndCommandBuffer(command_buffer) );
}

void WorkspaceManager::Workspace::reset_recoring(){
    VK( vkResetCommandBuffer(command_buffer, 0) );
}

WorkspaceManager::~WorkspaceManager() {
    if(command_pool != VK_NULL_HANDLE || descriptor_pool != VK_NULL_HANDLE) {
        std::cerr << "[WorkspaceManager] command_pool or descriptor_pool not properly destroyed" << std::endl;
    }

    for(auto &workspace : workspaces) {
        if(workspace.command_buffer != VK_NULL_HANDLE) {
            std::cerr << "[WorkspaceManager] workspace.command_buffer not properly destroyed" << std::endl;
        }

        for(auto &buffer_pair : workspace.buffer_pairs) {
            if(buffer_pair.descriptor != VK_NULL_HANDLE) {
                std::cerr << "[WorkspaceManager] workspace.buffer_pair.descriptor not properly destroyed" << std::endl; 
            }
        }
    }
}

void WorkspaceManager::create(RTG &rtg, std::vector<Pipeline::BlockDescriptorConfig> &pipeline_configs, uint32_t num_workspaces) {
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
        workspaces.emplace_back(std::move(Workspace{*this}));
        workspaces.back().create(rtg, pipeline_configs);
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

void WorkspaceManager::update_all_descriptors(RTG& rtg, std::vector<Pipeline::BlockDescriptorConfig> &pipeline_configs, uint32_t index, VkDeviceSize size) {
    for (auto& workspace : workspaces) {
        workspace.update_descriptor(rtg, pipeline_configs, index, size);
    }
}

void WorkspaceManager::copy_all_buffers(RTG& rtg, std::vector<Pipeline::BlockDescriptorConfig> &pipeline_configs, uint32_t index, VkDeviceSize size) {
    for (auto& workspace : workspaces) {
        workspace.copy_buffer(rtg, pipeline_configs, index, size);
    }
}
