#include "GBufferManager.hpp"

#include <array>
#include <cassert>
#include <iostream>

void GBufferManager::create(RTG &rtg, RenderPassManager &render_pass_manager, VkExtent2D const &extent) {
	destroy(rtg);

	assert(extent.width > 0 && extent.height > 0);
	assert(render_pass_manager.gbuffer_render_pass != VK_NULL_HANDLE);
	assert(render_pass_manager.depth_format != VK_FORMAT_UNDEFINED);

	depth_image = rtg.helpers.create_image(
		extent,
		render_pass_manager.depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped,
		0,
		1,
		1
	);

	albedo_image = rtg.helpers.create_image(
		extent,
		render_pass_manager.albedo_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped,
		0,
		1,
		1
	);

	normal_image = rtg.helpers.create_image(
		extent,
		render_pass_manager.normal_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped,
		0,
		1,
		1
	);

	pbr_image = rtg.helpers.create_image(
		extent,
		render_pass_manager.pbr_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped,
		0,
		1,
		1
	);

	{
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = depth_image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = render_pass_manager.depth_format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		VK(vkCreateImageView(rtg.device, &create_info, nullptr, &depth_view));
	}

	{
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = albedo_image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = render_pass_manager.albedo_format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		VK(vkCreateImageView(rtg.device, &create_info, nullptr, &albedo_view));
	}

	{
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = normal_image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = render_pass_manager.normal_format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		VK(vkCreateImageView(rtg.device, &create_info, nullptr, &normal_view));
	}

	{
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = pbr_image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = render_pass_manager.pbr_format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
		};
		VK(vkCreateImageView(rtg.device, &create_info, nullptr, &pbr_view));
	}

	{
		VkSamplerCreateInfo sampler_info{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = VK_FILTER_NEAREST,
			.minFilter = VK_FILTER_NEAREST,
			.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.mipLodBias = 0.0f,
			.anisotropyEnable = VK_FALSE,
			.maxAnisotropy = 1.0f,
			.compareEnable = VK_FALSE,
			.compareOp = VK_COMPARE_OP_ALWAYS,
			.minLod = 0.0f,
			.maxLod = 0.0f,
			.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
			.unnormalizedCoordinates = VK_FALSE,
		};
		VK(vkCreateSampler(rtg.device, &sampler_info, nullptr, &gbuffer_sampler));
	}

	{
		std::array<VkImageView, 4> attachments{
			albedo_view,
			normal_view,
			pbr_view,
			depth_view,
		};

		VkFramebufferCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = render_pass_manager.gbuffer_render_pass,
			.attachmentCount = uint32_t(attachments.size()),
			.pAttachments = attachments.data(),
			.width = extent.width,
			.height = extent.height,
			.layers = 1,
		};

		VK(vkCreateFramebuffer(rtg.device, &create_info, nullptr, &gbuffer_framebuffer));
	}
}

void GBufferManager::destroy(RTG &rtg) {
	if (gbuffer_framebuffer != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(rtg.device, gbuffer_framebuffer, nullptr);
		gbuffer_framebuffer = VK_NULL_HANDLE;
	}

	if (depth_view != VK_NULL_HANDLE) {
		vkDestroyImageView(rtg.device, depth_view, nullptr);
		depth_view = VK_NULL_HANDLE;
	}
	if (albedo_view != VK_NULL_HANDLE) {
		vkDestroyImageView(rtg.device, albedo_view, nullptr);
		albedo_view = VK_NULL_HANDLE;
	}
	if (normal_view != VK_NULL_HANDLE) {
		vkDestroyImageView(rtg.device, normal_view, nullptr);
		normal_view = VK_NULL_HANDLE;
	}
	if (pbr_view != VK_NULL_HANDLE) {
		vkDestroyImageView(rtg.device, pbr_view, nullptr);
		pbr_view = VK_NULL_HANDLE;
	}

	if (depth_image.handle != VK_NULL_HANDLE) {
		rtg.helpers.destroy_image(std::move(depth_image));
	}
	if (albedo_image.handle != VK_NULL_HANDLE) {
		rtg.helpers.destroy_image(std::move(albedo_image));
	}
	if (normal_image.handle != VK_NULL_HANDLE) {
		rtg.helpers.destroy_image(std::move(normal_image));
	}
	if (pbr_image.handle != VK_NULL_HANDLE) {
		rtg.helpers.destroy_image(std::move(pbr_image));
	}

	if (gbuffer_sampler != VK_NULL_HANDLE) {
		vkDestroySampler(rtg.device, gbuffer_sampler, nullptr);
		gbuffer_sampler = VK_NULL_HANDLE;
	}
}

std::array<VkDescriptorImageInfo, 4> GBufferManager::get_descriptor_image_infos() const {
	return {
		VkDescriptorImageInfo{
			.sampler = gbuffer_sampler,
			.imageView = depth_view,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		},
		VkDescriptorImageInfo{
			.sampler = gbuffer_sampler,
			.imageView = albedo_view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		},
		VkDescriptorImageInfo{
			.sampler = gbuffer_sampler,
			.imageView = normal_view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		},
		VkDescriptorImageInfo{
			.sampler = gbuffer_sampler,
			.imageView = pbr_view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		},
	};
}

GBufferManager::~GBufferManager() {
	if (gbuffer_framebuffer != VK_NULL_HANDLE) {
		std::cerr << "GBufferManager: gbuffer_framebuffer not destroyed" << std::endl;
	}
	if (depth_view != VK_NULL_HANDLE) {
		std::cerr << "GBufferManager: depth_view not destroyed" << std::endl;
	}
	if (albedo_view != VK_NULL_HANDLE) {
		std::cerr << "GBufferManager: albedo_view not destroyed" << std::endl;
	}
	if (normal_view != VK_NULL_HANDLE) {
		std::cerr << "GBufferManager: normal_view not destroyed" << std::endl;
	}
	if (pbr_view != VK_NULL_HANDLE) {
		std::cerr << "GBufferManager: pbr_view not destroyed" << std::endl;
	}
	if (depth_image.handle != VK_NULL_HANDLE) {
		std::cerr << "GBufferManager: depth_image not destroyed" << std::endl;
	}
	if (albedo_image.handle != VK_NULL_HANDLE) {
		std::cerr << "GBufferManager: albedo_image not destroyed" << std::endl;
	}
	if (normal_image.handle != VK_NULL_HANDLE) {
		std::cerr << "GBufferManager: normal_image not destroyed" << std::endl;
	}
	if (pbr_image.handle != VK_NULL_HANDLE) {
		std::cerr << "GBufferManager: pbr_image not destroyed" << std::endl;
	}
	if (gbuffer_sampler != VK_NULL_HANDLE) {
		std::cerr << "GBufferManager: gbuffer_sampler not destroyed" << std::endl;
	}
}
