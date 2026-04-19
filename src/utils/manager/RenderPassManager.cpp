#include "RenderPassManager.hpp"
#include "buffer/GBufferManager.hpp"
#include "buffer/HDRBufferManager.hpp"
#include "buffer/ShadowBufferManager.hpp"
#include "RTG.hpp"
#include <array>

void RenderPassManager::create(
	RTG& rtg,
	HDRBufferManager const& hdr_buffer_manager,
	GBufferManager const* gbuffer_manager,
	ShadowBufferManager const* shadow_buffer_manager
) {
	VkFormat const hdr_format = hdr_buffer_manager.hdr_format;
	VkFormat const main_depth_format = hdr_buffer_manager.depth_format;
	VkFormat const gbuffer_albedo_format = gbuffer_manager ? gbuffer_manager->albedo_format : VK_FORMAT_R8G8B8A8_UNORM;
	VkFormat const gbuffer_normal_format = gbuffer_manager ? gbuffer_manager->normal_format : VK_FORMAT_R16G16B16A16_SFLOAT;
	VkFormat const gbuffer_depth_format =
		(gbuffer_manager && gbuffer_manager->depth_format != VK_FORMAT_UNDEFINED)
			? gbuffer_manager->depth_format
			: main_depth_format;
	VkFormat const ao_format = gbuffer_manager ? gbuffer_manager->ao_format : VK_FORMAT_R8_UNORM;
	VkFormat const shadow_depth_format =
		(shadow_buffer_manager && shadow_buffer_manager->depth_format != VK_FORMAT_UNDEFINED)
			? shadow_buffer_manager->depth_format
			: main_depth_format;

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
				.format = main_depth_format,
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
				.format = main_depth_format,
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

	{ // Create GBuffer render pass (for deferred geometry writes)
		std::array< VkAttachmentDescription, 3 > gbuffer_attachments{
			VkAttachmentDescription{ // 0 - albedo
				.format = gbuffer_albedo_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
			VkAttachmentDescription{ // 1 - normal
				.format = gbuffer_normal_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
			VkAttachmentDescription{ // 2 - depth
				.format = gbuffer_depth_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			},
		};

		std::array<VkAttachmentReference, 2> gbuffer_color_refs{
			VkAttachmentReference{ .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
			VkAttachmentReference{ .attachment = 1, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		};

		VkAttachmentReference gbuffer_depth_ref{
			.attachment = 2,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription gbuffer_subpass{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = uint32_t(gbuffer_color_refs.size()),
			.pColorAttachments = gbuffer_color_refs.data(),
			.pDepthStencilAttachment = &gbuffer_depth_ref,
		};

		std::array< VkSubpassDependency, 2 > gbuffer_dependencies{
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

		VkRenderPassCreateInfo gbuffer_create_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = uint32_t(gbuffer_attachments.size()),
			.pAttachments = gbuffer_attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &gbuffer_subpass,
			.dependencyCount = uint32_t(gbuffer_dependencies.size()),
			.pDependencies = gbuffer_dependencies.data(),
		};

		VK( vkCreateRenderPass(rtg.device, &gbuffer_create_info, nullptr, &gbuffer_render_pass) );
	}

	{ // Create AO render pass (fullscreen AO output)
		std::array< VkAttachmentDescription, 1 > ao_attachments{
			VkAttachmentDescription{ // 0 - AO color attachment
				.format = ao_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
		};

		VkAttachmentReference ao_color_ref{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription ao_subpass{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 1,
			.pColorAttachments = &ao_color_ref,
			.pDepthStencilAttachment = nullptr,
		};

		std::array< VkSubpassDependency, 1 > ao_dependencies{
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			}
		};

		VkRenderPassCreateInfo ao_create_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = uint32_t(ao_attachments.size()),
			.pAttachments = ao_attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &ao_subpass,
			.dependencyCount = uint32_t(ao_dependencies.size()),
			.pDependencies = ao_dependencies.data(),
		};

		VK( vkCreateRenderPass(rtg.device, &ao_create_info, nullptr, &ao_render_pass) );
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

	{ // Create spot shadow render pass (depth only)
		std::array< VkAttachmentDescription, 1 > shadow_attachments{
			VkAttachmentDescription{ // 0 - depth attachment
				.format = shadow_depth_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			},
		};

		VkAttachmentReference shadow_depth_ref{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		};

		VkSubpassDescription shadow_subpass{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = nullptr,
			.colorAttachmentCount = 0,
			.pColorAttachments = nullptr,
			.pDepthStencilAttachment = &shadow_depth_ref,
		};

		std::array< VkSubpassDependency, 2 > shadow_dependencies{
			VkSubpassDependency{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			},
			VkSubpassDependency{
				.srcSubpass = 0,
				.dstSubpass = VK_SUBPASS_EXTERNAL,
				.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			},
		};

		VkRenderPassCreateInfo shadow_create_info{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = uint32_t(shadow_attachments.size()),
			.pAttachments = shadow_attachments.data(),
			.subpassCount = 1,
			.pSubpasses = &shadow_subpass,
			.dependencyCount = uint32_t(shadow_dependencies.size()),
			.pDependencies = shadow_dependencies.data(),
		};

		VK( vkCreateRenderPass(rtg.device, &shadow_create_info, nullptr, &shadow_render_pass) );
	}
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

	if (gbuffer_render_pass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(rtg.device, gbuffer_render_pass, nullptr);
		gbuffer_render_pass = VK_NULL_HANDLE;
	}

	if (ao_render_pass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(rtg.device, ao_render_pass, nullptr);
		ao_render_pass = VK_NULL_HANDLE;
	}

    if (tonemap_render_pass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(rtg.device, tonemap_render_pass, nullptr);
		tonemap_render_pass = VK_NULL_HANDLE;
	}

	if (shadow_render_pass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(rtg.device, shadow_render_pass, nullptr);
		shadow_render_pass = VK_NULL_HANDLE;
	}
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
	if(ao_render_pass != VK_NULL_HANDLE) {
		std::cerr << "[RenderPassManager] ao_render_pass not properly destroyed" << std::endl;
	}
	if(shadow_render_pass != VK_NULL_HANDLE) {
		std::cerr << "[RenderPassManager] spot_shadow_render_pass not properly destroyed" << std::endl;
	}
}