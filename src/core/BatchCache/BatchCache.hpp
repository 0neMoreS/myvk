#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

#include "PosVertex.hpp"

#include "RTG.hpp"

struct BatchCache : RTG::Application {

	BatchCache(RTG &);
	BatchCache(RTG &, uint32_t max_indices);
	BatchCache(BatchCache const &) = delete; //you shouldn't be copying this object
	~BatchCache();

	uint32_t MAX_INDICES;

	//kept for use in destructor:
	RTG &rtg;

	//--------------------------------------------------------------------
	//Resources that last the lifetime of the application:

	//chosen format for depth buffer:
	VkFormat depth_format{};
	//Render passes describe how pipelines write to images:
	VkRenderPass render_pass = VK_NULL_HANDLE;

	//Pipelines:

	struct BatchCachePipeline {
		//no descriptor set layouts

		VkPipelineLayout layout = VK_NULL_HANDLE;

		//no vertex bindings
		using Vertex = PosVertex;
		
		VkPipeline handle = VK_NULL_HANDLE;

		void create(RTG &, VkRenderPass render_pass, uint32_t subpass);
		void destroy(RTG &);
	} batchcache_pipeline;

	//pools from which per-workspace things are allocated:
	VkCommandPool command_pool = VK_NULL_HANDLE;
	VkQueryPool queryPool = VK_NULL_HANDLE;
	
	//STEPX: Add descriptor pool here.

	//workspaces hold per-render resources:
	struct Workspace {
		VkCommandBuffer command_buffer = VK_NULL_HANDLE; //from the command pool above; reset at the start of every render.
	};
	std::vector< Workspace > workspaces;

	//-------------------------------------------------------------------
	//static scene resources:
	//location for lines data: (streamed to GPU per-frame)
	Helpers::AllocatedBuffer vertices_buffer; //device-local
	Helpers::AllocatedBuffer indices_buffer; //device-local
	//--------------------------------------------------------------------
	//Resources that change when the swapchain is resized:

	virtual void on_swapchain(RTG &, RTG::SwapchainEvent const &) override;

	Helpers::AllocatedImage swapchain_depth_image;
	VkImageView swapchain_depth_image_view = VK_NULL_HANDLE;
	std::vector< VkFramebuffer > swapchain_framebuffers;
	//used from on_swapchain and the destructor: (framebuffers are created in on_swapchain)
	void destroy_framebuffers();

	//--------------------------------------------------------------------
	//Resources that change when time passes or the user interacts:

	virtual void update(float dt) override;
	virtual void on_input(InputEvent const &) override;

	std::vector< BatchCachePipeline::Vertex > vertices;
	std::vector< uint32_t > indices;

	//--------------------------------------------------------------------
	//Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
};
