#include "FrameBufferManager.hpp"

void FrameBufferManager::create(RTG &rtg, RTG::SwapchainEvent const &swapchain, RenderPassManager &render_pass_manager){
    // Clean up existing framebuffers
    destroy(rtg);

	// Create HDR color image
	hdr_color_image = rtg.helpers.create_image(
		swapchain.extent,
		render_pass_manager.hdr_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped
	);

	{ // Create HDR color image view
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = hdr_color_image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = render_pass_manager.hdr_format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};

		VK( vkCreateImageView(rtg.device, &create_info, nullptr, &hdr_color_image_view) );
	}

	// Create HDR depth image
	hdr_depth_image = rtg.helpers.create_image(
		swapchain.extent,
		render_pass_manager.depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped
	);

	{ // Create HDR depth image view
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = hdr_depth_image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = render_pass_manager.depth_format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};

		VK( vkCreateImageView(rtg.device, &create_info, nullptr, &hdr_depth_image_view) );
	}

	{ // Create HDR framebuffer
		std::array< VkImageView, 2 > hdr_attachments{
			hdr_color_image_view,
			hdr_depth_image_view,
		};

		VkFramebufferCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = render_pass_manager.hdr_render_pass,
			.attachmentCount = uint32_t(hdr_attachments.size()),
			.pAttachments = hdr_attachments.data(),
			.width = swapchain.extent.width,
			.height = swapchain.extent.height,
			.layers = 1,
		};

		VK( vkCreateFramebuffer(rtg.device, &create_info, nullptr, &hdr_framebuffer) );
	}

	{ // Create HDR sampler for tone mapping
		VkSamplerCreateInfo sampler_info{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_LINEAR,
			.minFilter = VK_FILTER_LINEAR,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_FALSE,
			.maxAnisotropy = 1.0f,
			.compareEnable = VK_FALSE,
			.minLod = 0.0f,
			.maxLod = 0.0f,
			.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
			.unnormalizedCoordinates = VK_FALSE,
		};

		VK( vkCreateSampler(rtg.device, &sampler_info, nullptr, &hdr_sampler) );
	}

	// Create swapchain framebuffers for tone mapping pass (no depth)
	swapchain_framebuffers.assign(swapchain.image_views.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < swapchain.image_views.size(); ++i) {
		std::array< VkImageView, 1 > attachments{
			swapchain.image_views[i],
		};

		VkFramebufferCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = render_pass_manager.tonemap_render_pass,
			.attachmentCount = uint32_t(attachments.size()),
			.pAttachments = attachments.data(),
			.width = swapchain.extent.width,
			.height = swapchain.extent.height,
			.layers = 1,
		};

		VK( vkCreateFramebuffer(rtg.device, &create_info, nullptr, &swapchain_framebuffers[i]) );
	}
}

void  FrameBufferManager::destroy(RTG &rtg){
    if(hdr_color_image.handle == VK_NULL_HANDLE){
        return;
    }

	// Destroy swapchain framebuffers
	for (VkFramebuffer &framebuffer : swapchain_framebuffers) {
		assert(framebuffer != VK_NULL_HANDLE);
		vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
		framebuffer = VK_NULL_HANDLE;
	}
	swapchain_framebuffers.clear();

	// Destroy HDR framebuffer
	if (hdr_framebuffer != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(rtg.device, hdr_framebuffer, nullptr);
		hdr_framebuffer = VK_NULL_HANDLE;
	}

	// Destroy HDR color image and view
	if (hdr_color_image_view != VK_NULL_HANDLE) {
		vkDestroyImageView(rtg.device, hdr_color_image_view, nullptr);
		hdr_color_image_view = VK_NULL_HANDLE;
	}
	rtg.helpers.destroy_image(std::move(hdr_color_image));

	// Destroy HDR depth image and view
	if (hdr_depth_image_view != VK_NULL_HANDLE) {
		vkDestroyImageView(rtg.device, hdr_depth_image_view, nullptr);
		hdr_depth_image_view = VK_NULL_HANDLE;
	}
	rtg.helpers.destroy_image(std::move(hdr_depth_image));

	// Destroy HDR sampler
	if (hdr_sampler != VK_NULL_HANDLE) {
		vkDestroySampler(rtg.device, hdr_sampler, nullptr);
		hdr_sampler = VK_NULL_HANDLE;
	}
}

FrameBufferManager::~FrameBufferManager(){
    for(VkFramebuffer &framebuffer : swapchain_framebuffers){
        if(framebuffer != VK_NULL_HANDLE){
            std::cerr << "FrameBufferManager: swapchain framebuffer not destroyed" << std::endl;
        }
    }
    if(hdr_framebuffer != VK_NULL_HANDLE){
        std::cerr << "FrameBufferManager: hdr_framebuffer not destroyed" << std::endl;
    }
    if(hdr_color_image_view != VK_NULL_HANDLE){
        std::cerr << "FrameBufferManager: hdr_color_image_view not destroyed" << std::endl;
    }
    if(hdr_depth_image_view != VK_NULL_HANDLE){
        std::cerr << "FrameBufferManager: hdr_depth_image_view not destroyed" << std::endl;
    }
    if(hdr_sampler != VK_NULL_HANDLE){
        std::cerr << "FrameBufferManager: hdr_sampler not destroyed" << std::endl;
    }
}