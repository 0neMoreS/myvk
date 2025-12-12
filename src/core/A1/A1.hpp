#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

#include "Vertex.hpp"
#include "s72.hpp"

#include "RTG.hpp"

#include <GLFW/glfw3.h>

struct A1 : RTG::Application {

	A1(RTG &);
	A1(RTG &, const std::string &);
	A1(A1 const &) = delete; //you shouldn't be copying this object
	~A1();

	//kept for use in destructor:
	RTG &rtg;
	s72::Document doc;

	//--------------------------------------------------------------------
	//Resources that last the lifetime of the application:

	//chosen format for depth buffer:
	VkFormat depth_format{};
	//Render passes describe how pipelines write to images:
	VkRenderPass render_pass = VK_NULL_HANDLE;

	//Pipelines:

	struct ObjectsPipeline {
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

		VkPipelineLayout layout = VK_NULL_HANDLE;
			
		VkPipeline handle = VK_NULL_HANDLE;

		void create(RTG &, VkRenderPass render_pass, uint32_t subpass);
		void destroy(RTG &);
	} objects_pipeline;

	//pools from which per-workspace things are allocated:
	VkCommandPool command_pool = VK_NULL_HANDLE;
	VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
	
	//STEPX: Add descriptor pool here.

	//workspaces hold per-render resources:
	struct Workspace {
		VkCommandBuffer command_buffer = VK_NULL_HANDLE; //from the command pool above; reset at the start of every render.

		//location for ObjectsPipeline::World data: (streamed to GPU per-frame)
		Helpers::AllocatedBuffer World_src; //host coherent; mapped
		Helpers::AllocatedBuffer World; //device-local
		VkDescriptorSet World_descriptors; //references World

		//location for ObjectsPipeline::Transforms data: (streamed to GPU per-frame)
		Helpers::AllocatedBuffer Transforms_src; //host coherent; mapped
		Helpers::AllocatedBuffer Transforms; //device-local
		VkDescriptorSet Transforms_descriptors; //references Transforms
	};
	std::vector< Workspace > workspaces;

	//-------------------------------------------------------------------
	//static scene resources:

	Helpers::AllocatedBuffer object_vertices;
	struct ObjectVertices {
		uint32_t first = 0;
		uint32_t count = 0;
	};
	std::vector<ObjectVertices> object_vertices_list;

	std::vector< Helpers::AllocatedImage > textures;
	std::vector< VkImageView > texture_views;
	VkSampler texture_sampler = VK_NULL_HANDLE;
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

	glm::mat4 PERSPECTIVE;
	glm::mat4 VIEW;

	// Camera control
	glm::vec3 camera_position{0.0f, 0.0f, -2.0f};
	glm::vec3 camera_up = glm::vec3{0.0f, -1.0f, 0.0f};
	float camera_theta = glm::radians(90.0f);
	float camera_phi = glm::radians(90.0f);
	float camera_fov = glm::radians(60.0f);
	float last_mouse_x = 0.0f;
	float last_mouse_y = 0.0f;
	bool keys_down[GLFW_KEY_LAST + 1] = {};
	const float move_speed = 2.0f;
	const float fov_speed = 1.0f;
	const float rotate_speed = 1.0f;

	ObjectsPipeline::World world;

	struct ObjectInstance {
		ObjectVertices vertices;
		ObjectsPipeline::Transform transform;
		uint32_t texture = 0;
	};
	std::vector< ObjectInstance > object_instances;

	//--------------------------------------------------------------------
	//Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
};
