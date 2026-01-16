#include "WorkspaceManager.hpp"

const std::unordered_map<VkDescriptorType, VkBufferUsageFlagBits> WorkspaceManager::descriptor_type_to_buffer_usage{{
    {VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VkBufferUsageFlagBits::VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
    {VkDescriptorType::VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkBufferUsageFlagBits::VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT}
}};

WorkspaceManager::Workspace::BufferPair::BufferPair(BufferPair&& other) noexcept
    : host(std::move(other.host)),
    device(std::move(other.device)) {}

WorkspaceManager::Workspace::BufferPair& WorkspaceManager::Workspace::BufferPair::operator=(BufferPair&& other) noexcept {
    if (this != &other) {
        host = std::move(other.host);
        device = std::move(other.device);
    }
    return *this;
}

WorkspaceManager::Workspace::DescriptorSetGroup::DescriptorSetGroup(DescriptorSetGroup&& other) noexcept
    : descriptor_set(std::move(other.descriptor_set)),
        buffer_pairs(std::move(other.buffer_pairs)) {
    other.descriptor_set = VK_NULL_HANDLE;
}

WorkspaceManager::Workspace::DescriptorSetGroup& WorkspaceManager::Workspace::DescriptorSetGroup::operator=(DescriptorSetGroup&& other) noexcept {
    if (this != &other) {
        descriptor_set = std::move(other.descriptor_set);
        buffer_pairs = std::move(other.buffer_pairs);
        other.descriptor_set = VK_NULL_HANDLE;
    }
    return *this;
};

WorkspaceManager::Workspace::Workspace(Workspace&& other) noexcept
    : command_buffer(std::move(other.command_buffer)),
        manager(std::move(other.manager)),
        pipeline_descriptor_set_groups(std::move(other.pipeline_descriptor_set_groups)),
        global_buffer_pairs(std::move(other.global_buffer_pairs)) {
    other.command_buffer = VK_NULL_HANDLE;
    other.manager = nullptr;
}

WorkspaceManager::Workspace& WorkspaceManager::Workspace::operator=(Workspace&& other) noexcept {
    if (this != &other) {
        command_buffer = std::move(other.command_buffer);
        manager = std::move(other.manager);
        pipeline_descriptor_set_groups = std::move(other.pipeline_descriptor_set_groups);
        global_buffer_pairs = std::move(other.global_buffer_pairs);
        other.manager = nullptr;
        other.command_buffer = VK_NULL_HANDLE;
    }
    return *this;
};

void WorkspaceManager::Workspace::create(RTG& rtg) {
    { // allocate one command buffer per workspace
        VkCommandBufferAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool =  manager->command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VK( vkAllocateCommandBuffers(rtg.device, &alloc_info, &command_buffer) );
    }

    // Create buffer pairs for each pipeline
    pipeline_descriptor_set_groups.resize(manager->block_descriptor_configs_by_pipeline.size());
    for (size_t pipeline_index = 0; pipeline_index < manager->block_descriptor_configs_by_pipeline.size(); ++pipeline_index) {
        pipeline_descriptor_set_groups[pipeline_index].resize(manager->block_descriptor_configs_by_pipeline[pipeline_index].size());
        auto& pipeline_configs = manager->block_descriptor_configs_by_pipeline[pipeline_index];

        for(size_t descriptor_set_index = 0; descriptor_set_index < pipeline_configs.size(); ++descriptor_set_index) {
            auto& config = pipeline_configs[descriptor_set_index];
            auto& descriptor_set_group = pipeline_descriptor_set_groups[pipeline_index][descriptor_set_index];

            { //allocate descriptor set for descriptor
                VkDescriptorSetAllocateInfo alloc_info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = manager->descriptor_pool,
                    .descriptorSetCount = 1,
                    .pSetLayouts = &config.layout,
                };

                VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &descriptor_set_group.descriptor_set) );
            }

            for(size_t binding_index = 0; binding_index < config.bindings_count; ++binding_index) {
                // Initialize empty BufferPair
                auto buffer_pair = std::make_shared<BufferPair>();
                descriptor_set_group.buffer_pairs.push_back(buffer_pair);
            }
        }
    }

    for(auto &global_buffer_config : manager->global_buffer_configs) {
        auto new_pair = std::make_shared<BufferPair>();

        new_pair->host = rtg.helpers.create_buffer(
            global_buffer_config.size, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            Helpers::Mapped
        );

        new_pair->device = rtg.helpers.create_buffer(
            global_buffer_config.size,
            global_buffer_config.usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            Helpers::Unmapped
        );

        global_buffer_pairs[global_buffer_config.name] = std::move(new_pair);
    }
}

void WorkspaceManager::Workspace::destroy(RTG &rtg) {
    if (command_buffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(rtg.device, manager->command_pool, 1, &command_buffer);
        command_buffer = VK_NULL_HANDLE;
    }

    manager = nullptr;

    for(auto& [name, buffer_pair] : global_buffer_pairs) {
        if (buffer_pair->host.handle != VK_NULL_HANDLE) {
            rtg.helpers.destroy_buffer(std::move(buffer_pair->host));
        }
        if (buffer_pair->device.handle != VK_NULL_HANDLE) {
            rtg.helpers.destroy_buffer(std::move(buffer_pair->device));
        }
    }

    global_buffer_pairs.clear();
    

    for(auto& descriptor_sets : pipeline_descriptor_set_groups){
        for(auto& descriptor_set_group : descriptor_sets){
            for(auto& buffer_pair : descriptor_set_group.buffer_pairs){
                if (buffer_pair->host.handle != VK_NULL_HANDLE) {
                    rtg.helpers.destroy_buffer(std::move(buffer_pair->host));
                }
                if (buffer_pair->device.handle != VK_NULL_HANDLE) {
                    rtg.helpers.destroy_buffer(std::move(buffer_pair->device));
                }
            }
            descriptor_set_group.descriptor_set = VK_NULL_HANDLE;
        }
    }

    pipeline_descriptor_set_groups.clear();
}

void WorkspaceManager::Workspace::update_descriptor(
    RTG& rtg, 
    uint32_t pipeline_index, 
    uint32_t descriptor_set_index, 
    uint32_t descriptor_index, 
    VkDeviceSize size
){
    auto& pipeline_descriptor_set_group = pipeline_descriptor_set_groups[pipeline_index][descriptor_set_index];
    auto& buffer_pair = pipeline_descriptor_set_group.buffer_pairs[descriptor_index];
    auto& config = manager->block_descriptor_configs_by_pipeline[pipeline_index][descriptor_set_index];

    if(buffer_pair->host.handle != VK_NULL_HANDLE) {
        rtg.helpers.destroy_buffer(std::move(buffer_pair->host));
    }
    if(buffer_pair->device.handle != VK_NULL_HANDLE) {
        rtg.helpers.destroy_buffer(std::move(buffer_pair->device));
    }

    // allocate data buffers:
    buffer_pair->host = rtg.helpers.create_buffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        Helpers::Mapped
    );
    buffer_pair->device = rtg.helpers.create_buffer(
        size,
        WorkspaceManager::descriptor_type_to_buffer_usage.at(config.type) | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped
    );

    { //point descriptor to the buffer:
        VkDescriptorBufferInfo buffer_info{
            .buffer = buffer_pair->device.handle,
            .offset = 0,
            .range = buffer_pair->device.size,
        };

        std::array< VkWriteDescriptorSet, 1 > writes{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = pipeline_descriptor_set_group.descriptor_set,
                .dstBinding = descriptor_index,
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

void WorkspaceManager::Workspace::update_global_descriptor(
    RTG& rtg, 
    uint32_t pipeline_index, 
    uint32_t descriptor_set_index, 
    uint32_t descriptor_index, 
    std::string buffer_name, 
    VkDeviceSize size
){
    auto& pipeline_descriptor_set_group = pipeline_descriptor_set_groups[pipeline_index][descriptor_set_index];
    auto& buffer_pair = global_buffer_pairs[buffer_name];
    auto& config = manager->block_descriptor_configs_by_pipeline[pipeline_index][descriptor_set_index];

    pipeline_descriptor_set_group.buffer_pairs[descriptor_index] = buffer_pair;

    { //point descriptor to the buffer:
        VkDescriptorBufferInfo buffer_info{
            .buffer = buffer_pair->device.handle,
            .offset = 0,
            .range = buffer_pair->device.size,
        };

        std::array< VkWriteDescriptorSet, 1 > writes{
            VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = pipeline_descriptor_set_group.descriptor_set,
                .dstBinding = descriptor_index,
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

void WorkspaceManager::Workspace::write_buffer(
    RTG& rtg, 
    uint32_t pipeline_index, 
    uint32_t descriptor_set_index, 
    uint32_t descriptor_index, 
    void* data, 
    VkDeviceSize size
){
    auto& pipeline_descriptor_set_group = pipeline_descriptor_set_groups[pipeline_index][descriptor_set_index];
    auto& buffer_pair = pipeline_descriptor_set_group.buffer_pairs[descriptor_index];

    memcpy(buffer_pair->host.allocation.data(), data, size);

    VkBufferCopy copy_region{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = size,
    };
    vkCmdCopyBuffer(command_buffer, buffer_pair->host.handle, buffer_pair->device.handle, 1, &copy_region);
}

void WorkspaceManager::Workspace::write_global_buffer(
    RTG& rtg, 
    std::string buffer_name, 
    void* data, 
    VkDeviceSize size
){
    auto& buffer_pair = global_buffer_pairs[buffer_name];

    memcpy(buffer_pair->host.allocation.data(), data, size);

    VkBufferCopy copy_region{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = size,
    };
    vkCmdCopyBuffer(command_buffer, buffer_pair->host.handle, buffer_pair->device.handle, 1, &copy_region);
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

void WorkspaceManager::Workspace::reset_recording(){
    VK( vkResetCommandBuffer(command_buffer, 0) );
}

void WorkspaceManager::create(
    RTG& rtg, 
    const std::vector<std::vector<Pipeline::BlockDescriptorConfig>> &&block_descriptor_configs_by_pipeline_, 
    const std::vector<GlobalBufferConfig> &&global_buffer_configs_, 
    uint32_t num_workspaces
) {
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
        // Count all descriptor types from all pipelines
        for (const auto& pipeline_configs : block_descriptor_configs_by_pipeline_) {
            for (const auto& config : pipeline_configs) {
                descriptor_map[config.type]++;
            }
        }

        uint32_t per_workspace = num_workspaces;

        std::vector<VkDescriptorPoolSize> pool_sizes;
        for (auto [type, count] : descriptor_map) {
            pool_sizes.emplace_back(
                VkDescriptorPoolSize{
                    .type = type,
                    .descriptorCount = count * per_workspace,
                });
        }

        // Calculate total descriptor sets needed
        uint32_t total_descriptors = 0;
        for (const auto& pipeline_configs : block_descriptor_configs_by_pipeline_) {
            total_descriptors += static_cast<uint32_t>(pipeline_configs.size());
        }

        VkDescriptorPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, //because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = total_descriptors * per_workspace,
			.poolSizeCount = uint32_t(pool_sizes.size()),
			.pPoolSizes = pool_sizes.data(),
		};

		VK( vkCreateDescriptorPool(rtg.device, &create_info, nullptr, &descriptor_pool) );
    }

    this->block_descriptor_configs_by_pipeline = std::move(block_descriptor_configs_by_pipeline_);
    this->global_buffer_configs = std::move(global_buffer_configs_);

    for(uint32_t i = 0; i < num_workspaces; i++) {
        workspaces.emplace_back(std::move(Workspace{*this}));
        workspaces.back().create(rtg);
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

void WorkspaceManager::write_all_buffers(
    RTG& rtg, 
    uint32_t pipeline_index, 
    uint32_t descriptor_set_index, 
    uint32_t descriptor_index, 
    void* data, 
    VkDeviceSize size
) {
    for (auto& workspace : workspaces) {
        workspace.write_buffer(rtg, pipeline_index, descriptor_set_index, descriptor_index, data, size);
    }
}

void WorkspaceManager::write_all_global_buffers(
    RTG& rtg, 
    std::string buffer_name, 
    void* data, 
    VkDeviceSize size
) {
    for (auto& workspace : workspaces) {
        workspace.write_global_buffer(rtg, buffer_name, data, size);
    }
}

void WorkspaceManager::update_all_descriptors(
    RTG& rtg, 
    uint32_t pipeline_index, 
    uint32_t descriptor_set_index, 
    uint32_t descriptor_index, 
    VkDeviceSize size
) {
    for (auto& workspace : workspaces) {
        workspace.update_descriptor(rtg, pipeline_index, descriptor_set_index, descriptor_index, size);
    }
}

void WorkspaceManager::update_all_global_descriptors(
    RTG& rtg, 
    uint32_t pipeline_index, 
    uint32_t descriptor_set_index, 
    uint32_t descriptor_index, 
    std::string buffer_name, 
    VkDeviceSize size
) {
    for (auto& workspace : workspaces) {
        workspace.update_global_descriptor(rtg, pipeline_index, descriptor_set_index, descriptor_index, buffer_name, size);
    }
}

WorkspaceManager::~WorkspaceManager() {
    assert(command_pool == VK_NULL_HANDLE);
    assert(descriptor_pool == VK_NULL_HANDLE);
}

WorkspaceManager::Workspace::~Workspace(){
    assert(manager == nullptr);
    assert(command_buffer == VK_NULL_HANDLE); 
}

WorkspaceManager::Workspace::BufferPair::~BufferPair(){
    assert(host.handle == VK_NULL_HANDLE);
    assert(device.handle == VK_NULL_HANDLE);
}

WorkspaceManager::Workspace::DescriptorSetGroup::~DescriptorSetGroup(){
    assert(descriptor_set == VK_NULL_HANDLE);
    buffer_pairs.clear();
}