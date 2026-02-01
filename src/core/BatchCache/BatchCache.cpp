#ifdef _WIN32
//ensure we have M_PI
#define _USE_MATH_DEFINES
#endif

#include "BatchCache.hpp"

#include "VK.hpp"
#include "refsol.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

BatchCache::BatchCache(RTG &rtg_) : BatchCache(rtg_, 300000) {
	//delegates to the other constructor
}

BatchCache::BatchCache(RTG &rtg_, uint32_t max_indices) : rtg(rtg_), MAX_INDICES(max_indices) {
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

	{ //create command pool
		VkCommandPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = rtg.graphics_queue_family.value(),
		};
		VK( vkCreateCommandPool(rtg.device, &create_info, nullptr, &command_pool) );
	}

	{ // create query pool
		VkQueryPoolCreateInfo queryPoolInfo{
			.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
			.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS,
			.queryCount = 1,
			.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT
		};
		VK( vkCreateQueryPool(rtg.device, &queryPoolInfo, nullptr, &query_pool) );
	}

	batchcache_pipeline.create(rtg, render_pass, 0);

	workspaces.resize(rtg.workspaces.size());
	for (Workspace &workspace : workspaces) {
		{ //allocate command buffer:
			VkCommandBufferAllocateInfo alloc_info{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = command_pool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};
			VK( vkAllocateCommandBuffers(rtg.device, &alloc_info, &workspace.command_buffer) );
		}

	}

	{ // Three vertices and a bunch of indexes
		vertices.push_back( {{-0.5f, -0.5f, 0.0f}} );
		vertices.push_back( {{ 0.5f, -0.5f, 0.0f}} );
		vertices.push_back( {{ 0.0f,  0.5f, 0.0f}} );
		
		indices.resize(max_indices);
		for (size_t i = 0; i < max_indices; i++) {
			indices[i] = i % 3;
		}
	}

	{ // create and copy buffers
		size_t vertices_bytes = sizeof(vertices[0]) * vertices.size();
		size_t indices_bytes = sizeof(indices[0]) * indices.size();

		vertices_buffer = rtg.helpers.create_buffer(
			vertices_bytes,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);
		indices_buffer = rtg.helpers.create_buffer(
			indices_bytes,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);

		rtg.helpers.transfer_to_buffer(vertices.data(), vertices_bytes, vertices_buffer);
		rtg.helpers.transfer_to_buffer(indices.data(), indices_bytes, indices_buffer);
	}
}

BatchCache::~BatchCache() {
	//just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) {
		std::cerr << "Failed to vkDeviceWaitIdle in BatchCache::~BatchCache [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	if (swapchain_depth_image.handle != VK_NULL_HANDLE) {
		destroy_framebuffers();
	}

	for (Workspace &workspace : workspaces) {
		if (workspace.command_buffer != VK_NULL_HANDLE) {
			vkFreeCommandBuffers(rtg.device, command_pool, 1, &workspace.command_buffer);
			workspace.command_buffer = VK_NULL_HANDLE;
		}
		//Transforms_descriptors freed when pool is destroyed.
	}
	workspaces.clear();

	batchcache_pipeline.destroy(rtg);

	if (command_pool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(rtg.device, command_pool, nullptr);
		command_pool = VK_NULL_HANDLE;
	}

	if( query_pool != VK_NULL_HANDLE) {
		vkDestroyQueryPool(rtg.device, query_pool, nullptr);
		query_pool = VK_NULL_HANDLE;
	}

	if (render_pass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(rtg.device, render_pass, nullptr);
		render_pass = VK_NULL_HANDLE;
	}

	rtg.helpers.destroy_buffer(std::move(vertices_buffer));
	rtg.helpers.destroy_buffer(std::move(indices_buffer));
}

void BatchCache::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain) {
	//[re]create framebuffers:
	//clean up existing framebuffers (and depth image):
	if (swapchain_depth_image.handle != VK_NULL_HANDLE) {
		destroy_framebuffers();
	}

	//Allocate depth image for framebuffers to share:
	swapchain_depth_image = rtg.helpers.create_image(
		swapchain.extent,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		Helpers::Unmapped
	);

	{ //create depth image view:
		VkImageViewCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_depth_image.handle,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = depth_format,
			.subresourceRange{
				.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1
			},
		};

		VK( vkCreateImageView(rtg.device, &create_info, nullptr, &swapchain_depth_image_view) );
	}

	//Make framebuffers for each swapchain image:
	swapchain_framebuffers.assign(swapchain.image_views.size(), VK_NULL_HANDLE);
	for (size_t i = 0; i < swapchain.image_views.size(); ++i) {
		std::array< VkImageView, 2 > attachments{
			swapchain.image_views[i],
			swapchain_depth_image_view,
		};
		VkFramebufferCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass = render_pass,
			.attachmentCount = uint32_t(attachments.size()),
			.pAttachments = attachments.data(),
			.width = swapchain.extent.width,
			.height = swapchain.extent.height,
			.layers = 1,
		};

		VK( vkCreateFramebuffer(rtg.device, &create_info, nullptr, &swapchain_framebuffers[i]) );
	}
}

void BatchCache::destroy_framebuffers() {
	for (VkFramebuffer &framebuffer : swapchain_framebuffers) {
		assert(framebuffer != VK_NULL_HANDLE);
		vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
		framebuffer = VK_NULL_HANDLE;
	}
	swapchain_framebuffers.clear();

	assert(swapchain_depth_image_view != VK_NULL_HANDLE);
	vkDestroyImageView(rtg.device, swapchain_depth_image_view, nullptr);
	swapchain_depth_image_view = VK_NULL_HANDLE;

	rtg.helpers.destroy_image(std::move(swapchain_depth_image));
}


void BatchCache::render(RTG &rtg_, RTG::RenderParams const &render_params) {
	//assert that parameters are valid:
	assert(&rtg == &rtg_);
	assert(render_params.workspace_index < workspaces.size());
	assert(render_params.image_index < swapchain_framebuffers.size());

	//get more convenient names for the current workspace and target framebuffer:
	Workspace &workspace = workspaces[render_params.workspace_index];
	VkFramebuffer framebuffer = swapchain_framebuffers[render_params.image_index];

	//record (into `workspace.command_buffer`) commands that run a `render_pass` that just clears `framebuffer`:
	VK( vkResetCommandBuffer(workspace.command_buffer, 0) );
	
	{ //begin recording:
		VkCommandBufferBeginInfo command_buffer_begin_info{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, //will record again every submit
		};
		
		VK( vkBeginCommandBuffer(workspace.command_buffer, &command_buffer_begin_info) );

		//render pass
		{
			std::array< VkClearValue, 2 > clear_values{
				VkClearValue{ .color{ .float32{0.0f, 0.0f, 0.0f, 1.0f} } },
				VkClearValue{ .depthStencil{ .depth = 1.0f, .stencil = 0 } },
			};
			
			VkRenderPassBeginInfo begin_info{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = render_pass,
				.framebuffer = framebuffer,
				.renderArea{
					.offset = {.x = 0, .y = 0},
					.extent = rtg.swapchain_extent,
				},
				.clearValueCount = uint32_t(clear_values.size()),
				.pClearValues = clear_values.data(),
			};

			vkCmdResetQueryPool(workspace.command_buffer, query_pool, 0, 1);

			vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
			{
				// run pipelines here
				{ //set scissor rectangle:
					VkRect2D scissor{
						.offset = {.x = 0, .y = 0},
						.extent = rtg.swapchain_extent,
					};
					vkCmdSetScissor(workspace.command_buffer, 0, 1, &scissor);
				}
				{ //configure viewport transform:
					VkViewport viewport{
						.x = 0.0f,
						.y = 0.0f,
						.width = float(rtg.swapchain_extent.width),
						.height = float(rtg.swapchain_extent.height),
						.minDepth = 0.0f,
						.maxDepth = 1.0f,
					};
					vkCmdSetViewport(workspace.command_buffer, 0, 1, &viewport);
				}


				{ //draw with the lines pipeline:
					vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, batchcache_pipeline.handle);

					{ //use vertices (offset 0) as vertex buffer binding 0:
						std::array< VkBuffer, 1 > vertex_buffers{ vertices_buffer.handle };
						std::array< VkDeviceSize, 1 > offsets{ 0 };
						vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
					}

					//bind index buffer:
                	vkCmdBindIndexBuffer(workspace.command_buffer, indices_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

					vkCmdBeginQuery(workspace.command_buffer, query_pool, 0, 0);

					{//draw lines vertices:
						vkCmdDrawIndexed(workspace.command_buffer, uint32_t(indices.size()), 1, 0, 0, 0);
					}
					
					vkCmdEndQuery(workspace.command_buffer, query_pool, 0);
				}
			}

			vkCmdEndRenderPass(workspace.command_buffer);
		}

		//end recording:
		VK( vkEndCommandBuffer(workspace.command_buffer) );
	}

	{//submit `workspace.command buffer` for the GPU to run:
		std::array< VkSemaphore, 1 > wait_semaphores{
			render_params.image_available
		};
		std::array< VkPipelineStageFlags, 1 > wait_stages{
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		};
		static_assert(wait_semaphores.size() == wait_stages.size(), "every semaphore needs a stage");

		std::array< VkSemaphore, 1 > signal_semaphores{
			render_params.image_done
		};
		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = uint32_t(wait_semaphores.size()),
			.pWaitSemaphores = wait_semaphores.data(),
			.pWaitDstStageMask = wait_stages.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &workspace.command_buffer,
			.signalSemaphoreCount = uint32_t(signal_semaphores.size()),
			.pSignalSemaphores = signal_semaphores.data(),
		};

		VK( vkQueueSubmit(rtg.graphics_queue, 1, &submit_info, render_params.workspace_available) );
	}

	uint64_t query_result = 0;
		VK( vkGetQueryPoolResults(
			rtg.device,
			query_pool,
			0, 1,
			sizeof(query_result),
			&query_result,
			sizeof(query_result),
			VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
		) );
	std::cout << "VS invocations: " << query_result << std::endl;
}


void BatchCache::update(float dt) {
}


void BatchCache::on_input(InputEvent const & event) {
}
