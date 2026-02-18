#pragma once

#include <vulkan/vulkan_core.h>

#include <vector>

struct RTG;

struct Helpers {

	//-----------------------
	//memory allocation:

	//An owning reference to (part of) a slab of device memory:
	struct Allocation {
		VkDeviceMemory handle = VK_NULL_HANDLE;
		VkDeviceSize offset = 0; //offset of the allocated object inside the memory
		VkDeviceSize size = 0; //size of the allocated object inside the memory (might be *larger* than the internal size of the object!)
		void *mapped = nullptr;
		void *data() const { return reinterpret_cast< char * >(mapped) + offset; } //get pointer to beginning of allocation, taking offset into account

		//Call an all-zero (no handle, offset, size, mapped) Allocation "empty":
		Allocation() = default; //default-constructed Allocation is empty
		Allocation(Allocation &&); //swaps contents with moved-from Allocation.
		Allocation &operator=(Allocation &&); //if *this is not empty, complains. Swaps contents with moved-from Allocation.
		~Allocation(); //complains if *this is not empty
	};

	enum MapFlag {
		Unmapped = 0,
		Mapped = 1,
	};

	//allocate a block of requested size and alignment from a memory with the given type index:
	Allocation allocate(VkDeviceSize size, VkDeviceSize alignment, uint32_t memory_type_index, MapFlag map = Unmapped);

	//allocate a block that works for a given VkMemoryRequirements and VkMemoryPropertyFlags:
	Allocation allocate(VkMemoryRequirements const &requirements, VkMemoryPropertyFlags memory_properties, MapFlag map = Unmapped);

	//free an allocated block:
	void free(Allocation &&allocation);

	//specializations that also create a buffer or image (respectively):
	struct AllocatedBuffer {
		VkBuffer handle = VK_NULL_HANDLE;
		VkDeviceSize size = 0; //bytes in the buffer (allocation.size might be larger)
		Allocation allocation;

		//NOTE: could define default constructor, move constructor, move assignment, destructor for a bit more paranoia
	};
	AllocatedBuffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, MapFlag map = Unmapped);
	void destroy_buffer(AllocatedBuffer &&allocated_buffer);

	struct AllocatedImage {
		VkImage handle = VK_NULL_HANDLE;
		VkExtent2D extent{.width = 0, .height = 0};
		VkFormat format = VK_FORMAT_UNDEFINED;
		Allocation allocation;

		//NOTE: could define default constructor, move constructor, move assignment, destructor for a bit more paranoia
	};
	// is_cube: if true, creates a cubemap-compatible image (6 array layers, cube flag set); if false, creates 2D image (1 layer)
	AllocatedImage create_image(
		VkExtent2D const &extent, 
		VkFormat format, 
		VkImageTiling tiling, 
		VkImageUsageFlags usage, 
		VkMemoryPropertyFlags properties, 
		MapFlag map = Unmapped, 
		bool is_cube = false,
		uint32_t mipmap_levels = 1
	);
	void destroy_image(AllocatedImage &&allocated_image);
	

	//-----------------------
	//CPU -> GPU data transfer:

	// NOTE: synchronizes *hard* against the GPU; inefficient to use for streaming data!
	void transfer_to_buffer(void *data, size_t size, AllocatedBuffer &target);
	// face_count: 1 for regular 2D images, 6 for cubemaps. data is face_count * (width*height*bytes_per_pixel) bytes
	// void transfer_to_image(void *data, size_t size, AllocatedImage &image, uint32_t face_count = 1); //NOTE: image layout after call is VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	void transfer_to_image(
		const std::vector<void*>& mipmap_data,
		const std::vector<size_t>& mipmap_sizes,
		AllocatedImage& target,
		uint32_t face_count
	);

	// Transition image layout (useful for storage images, transfer operations, etc.)
	// Uses transfer_command_buffer to submit a one-time command.
	// Common use cases:
	//   - UNDEFINED -> GENERAL for storage images
	//   - GENERAL -> TRANSFER_SRC_OPTIMAL for readback
	void transition_image_layout(
		VkImage image,
		VkImageLayout old_layout,
		VkImageLayout new_layout,
		uint32_t mip_levels = 1,
		uint32_t array_layers = 1
	);

	VkCommandPool transfer_command_pool = VK_NULL_HANDLE;
	VkCommandBuffer transfer_command_buffer = VK_NULL_HANDLE;
	//-----------------------
	//Misc utilities:

	//for selecting memory types (used by allocate, above):
	VkPhysicalDeviceMemoryProperties memory_properties{};
	uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags flags) const;

	//for selecting image formats:
	VkFormat find_image_format(std::vector< VkFormat > const &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

	//shader code from buffers -> modules:
	VkShaderModule create_shader_module(uint32_t const *code, size_t bytes) const;
	//version that figures out the size from a static array:
	template< size_t N >
	VkShaderModule create_shader_module(uint32_t const (&arr)[N]) const {
		return create_shader_module(arr, 4*N);
	}

	//-----------------------
	//internals:
	Helpers(RTG const &);
	Helpers(Helpers const &) = delete; //you shouldn't be copying Helpers
	~Helpers();
	RTG const &rtg; //remember the owning RTG object

	//used to synchronize create/destroy with RTG:
	void create(); //create vulkan resources (after GPU-held handles are created)
	void destroy(); //destroy vulkan resources (before GPU-held handles are destroyed)

private:
	// Helper: record image layout transition barrier into command buffer (does not submit)
	void record_image_layout_transition(
		VkCommandBuffer cmd_buffer,
		VkImage image,
		VkImageLayout old_layout,
		VkImageLayout new_layout,
		uint32_t mip_levels,
		uint32_t array_layers
	);
};
