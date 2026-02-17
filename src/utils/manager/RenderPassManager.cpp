#include "RenderPassManager.hpp"
#include "RTG.hpp"
#include <array>

void RenderPassManager::create(RTG& rtg, float aspect) {
    //select a depth format:
	//  (at least one of these two must be supported, according to the spec; but neither are required)
	depth_format = rtg.helpers.find_image_format(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32 },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);

	{ //create render pass
		//attachments
		std::array< VkAttachmentDescription, 2 > attachments{
			VkAttachmentDescription{ //0 - color attachment:
				.format = rtg.surface_format.format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = rtg.present_layout,
			},
			VkAttachmentDescription{ //1 - depth attachment:
				.format = depth_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		};

		//subpass
		VkAttachmentReference color_attachment_ref{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkAttachmentReference depth_attachment_ref{
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription subpass{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_ref,
			.pDepthStencilAttachment = &depth_attachment_ref,
		};

		//dependencies
		//this defers the image load actions for the attachments:
		std::array< VkSubpassDependency, 2 > dependencies {
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			},
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			}
		};

		VkRenderPassCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = uint32_t(attachments.size()),
			.pAttachments = attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = uint32_t(dependencies.size()),
			.pDependencies = dependencies.data(),
		};

		VK( vkCreateRenderPass(rtg.device, &create_info, nullptr, &render_pass) );
	}

    { // Create HDR render pass (for scene rendering to HDR texture)
		// Attachments
		std::array< VkAttachmentDescription, 2 > hdr_attachments{
			VkAttachmentDescription{ // 0 - HDR color attachment
				.format = hdr_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
			VkAttachmentDescription{ // 1 - depth attachment
				.format = depth_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		};

		// Subpass
		VkAttachmentReference hdr_color_ref{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkAttachmentReference hdr_depth_ref{
			.attachment = 1,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription hdr_subpass{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 1,
			.pColorAttachments = &hdr_color_ref,
			.pDepthStencilAttachment = &hdr_depth_ref,
		};

		// Dependencies
		std::array< VkSubpassDependency, 2 > hdr_dependencies{
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			},
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			}
		};

		VkRenderPassCreateInfo hdr_create_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = uint32_t(hdr_attachments.size()),
			.pAttachments = hdr_attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &hdr_subpass,
			.dependencyCount = uint32_t(hdr_dependencies.size()),
			.pDependencies = hdr_dependencies.data(),
		};

		VK( vkCreateRenderPass(rtg.device, &hdr_create_info, nullptr, &hdr_render_pass) );
	}

    { // Create tone mapping render pass (for rendering to swapchain)
		// Attachments
		std::array< VkAttachmentDescription, 1 > tonemap_attachments{
			VkAttachmentDescription{ // 0 - swapchain color attachment
				.format = rtg.surface_format.format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = rtg.present_layout,
			},
		};

		// Subpass
		VkAttachmentReference tonemap_color_ref{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription tonemap_subpass{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 1,
			.pColorAttachments = &tonemap_color_ref,
			.pDepthStencilAttachment = nullptr,
		};

		// Dependencies
		std::array< VkSubpassDependency, 1 > tonemap_dependencies{
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			}
		};

		VkRenderPassCreateInfo tonemap_create_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = uint32_t(tonemap_attachments.size()),
			.pAttachments = tonemap_attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &tonemap_subpass,
			.dependencyCount = uint32_t(tonemap_dependencies.size()),
			.pDependencies = tonemap_dependencies.data(),
		};

		VK( vkCreateRenderPass(rtg.device, &tonemap_create_info, nullptr, &tonemap_render_pass) );
	}

	{ // clears
		clears = {
			VkClearValue{ .color{ .float32{63.0f/255.0f, 63.0f/255.0f, 63.0f/255.0f, 1.0f} } },
			VkClearValue{ .depthStencil{ .depth = 1.0f, .stencil = 0 } },
		};

		tonemap_clears = {
			VkClearValue{ .color{ .float32{0.0f, 0.0f, 0.0f, 1.0f} } },
		};
	}

	{ // clear_center_attachment
		clear_center_attachment = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.colorAttachment = 0,
			.clearValue = VkClearValue{ .color{ .float32{0.0f, 0.0f, 0.0f, 1.0f} } },
		};

		clear_center_rect = {
			.rect = scissor,
			.baseArrayLayer = 0,
			.layerCount = 1,
		};
	}

	update_scissor_and_viewport(rtg, rtg.swapchain_extent, aspect);
}

void RenderPassManager::destroy(RTG& rtg) {
	if (render_pass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(rtg.device, render_pass, nullptr);
		render_pass = VK_NULL_HANDLE;
	}

    if (hdr_render_pass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(rtg.device, hdr_render_pass, nullptr);
		hdr_render_pass = VK_NULL_HANDLE;
	}

    if (tonemap_render_pass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(rtg.device, tonemap_render_pass, nullptr);
		tonemap_render_pass = VK_NULL_HANDLE;
	}
}

void RenderPassManager::update_scissor_and_viewport(RTG& rtg, VkExtent2D const& extent, float aspect) {
	const float swap_aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);

	uint32_t w = extent.width;
	uint32_t h = extent.height;

	if (swap_aspect >= aspect) {
		w = static_cast<uint32_t>(std::round(h * aspect));
	} else {
		h = static_cast<uint32_t>(std::round(w / aspect));
	}

	int32_t offset_x = (static_cast<int32_t>(extent.width) - static_cast<int32_t>(w)) / 2;
    int32_t offset_y = (static_cast<int32_t>(extent.height) - static_cast<int32_t>(h)) / 2;
	
	// scissor
	scissor = {
		.offset = {.x = offset_x, .y = offset_y},
		.extent = VkExtent2D{w, h},
	};

	//viewport
	viewport = {
		.x = float(offset_x),
		.y = float(offset_y),
		.width = float(w),
		.height = float(h),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	clear_center_rect.rect = scissor;
}

RenderPassManager::~RenderPassManager() {
	if(render_pass != VK_NULL_HANDLE) {
        std::cerr << "[RenderPassManager] render_pass not properly destroyed" << std::endl;
    }
    if(hdr_render_pass != VK_NULL_HANDLE) {
        std::cerr << "[RenderPassManager] hdr_render_pass not properly destroyed" << std::endl;
    }
    if(tonemap_render_pass != VK_NULL_HANDLE) {
        std::cerr << "[RenderPassManager] tonemap_render_pass not properly destroyed" << std::endl;
    }
}