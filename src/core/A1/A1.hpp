#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

#include "Vertex.hpp"
#include "S72Loader.hpp"
#include "Texture2DLoader.hpp"
#include "CameraManager.hpp"
#include "WorkspaceManager.hpp"
#include "RenderPassManager.hpp"
#include "Pipeline.hpp"
#include "VK.hpp"

#include "RTG.hpp"

#include <GLFW/glfw3.h>

struct A1 : RTG::Application {

	A1(RTG &);
	A1(RTG &, const std::string &);
	A1(A1 const &) = delete; //you shouldn't be copying this object
	~A1();

	//kept for use in destructor:
	RTG &rtg;
	std::shared_ptr<S72Loader::Document> doc;
	CameraManager camera_manager;
	WorkspaceManager workspace_manager;
	RenderPassManager render_pass_manager;

	const std::string s72_dir = "./external/s72/examples/";

	//--------------------------------------------------------------------
	//Resources that last the lifetime of the application:

	//Pipelines:

	struct ObjectsPipeline : Pipeline {
		//descriptor set layouts:
		
		VkDescriptorSetLayout set0_World = VK_NULL_HANDLE;
		VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;
		VkDescriptorSetLayout set2_TEXTURE = VK_NULL_HANDLE;

		//types for descriptors:
		struct World {
			struct { float x, y, z, padding_; } SKY_DIRECTION;
			struct { float r, g, b, padding_; } SKY_ENERGY;
			struct { float x, y, z, padding_; } SUN_DIRECTION;
			struct { float r, g, b, padding_; } SUN_ENERGY;
		};
		static_assert(sizeof(World) == 4*4 + 4*4 + 4*4 + 4*4, "World is the expected size.");

		//types for descriptors:
		struct Transform {
			glm::mat4 PERSPECTIVE;
			glm::mat4 VIEW;
			glm::mat4 MODEL;
			glm::mat4 MODEL_NORMAL;
		};
		static_assert(sizeof(Transform) == 16*4 + 16*4 + 16*4 + 16*4, "Transform is the expected size.");

		//no push constants
		void create(RTG &, VkRenderPass render_pass, uint32_t subpass) override;
		void destroy(RTG &) override;
	} objects_pipeline;

	//-------------------------------------------------------------------
	//static scene resources:

	Helpers::AllocatedBuffer object_vertices;
	struct ObjectVertices {
		uint32_t first = 0;
		uint32_t count = 0;
	};
	std::vector<ObjectVertices> object_vertices_list;

	std::vector< std::shared_ptr<Texture2DLoader::Texture> > textures;
	// std::vector< Helpers::AllocatedImage > textures;
	// std::vector< VkImageView > texture_views;
	// VkSampler texture_sampler = VK_NULL_HANDLE;
	VkDescriptorPool texture_descriptor_pool = VK_NULL_HANDLE;
	std::vector< VkDescriptorSet > texture_descriptors; //allocated from texture_descriptor_pool In the code we just wrote, te

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

	float time = 0.0f;

	ObjectsPipeline::World world;

	struct ObjectInstance {
		ObjectVertices vertices;
		ObjectsPipeline::Transform transform;
		size_t texture = 0;
	};
	std::vector< ObjectInstance > object_instances;

	//--------------------------------------------------------------------
	//Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
};
