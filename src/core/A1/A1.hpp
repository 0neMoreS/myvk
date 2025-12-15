#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

#include "S72Loader.hpp"
#include "Texture2DLoader.hpp"
#include "CameraManager.hpp"
#include "WorkspaceManager.hpp"
#include "RenderPassManager.hpp"
#include "A1ObjectsPipeline.hpp"
#include "SceneManager.hpp"
#include "TextureManager.hpp"
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

	//--------------------------------------------------------------------
	//Resources that last the lifetime of the application:

	A1ObjectsPipeline objects_pipeline;

	//-------------------------------------------------------------------
	//static scene resources:

	SceneManager scene_manager;
	TextureManager texture_manager;

	// std::vector< std::shared_ptr<Texture2DLoader::Texture> > textures;
	// std::vector< Helpers::AllocatedImage > textures;
	// std::vector< VkImageView > texture_views;
	// VkSampler texture_sampler = VK_NULL_HANDLE;
	// VkDescriptorPool texture_descriptor_pool = VK_NULL_HANDLE;
	// std::vector< VkDescriptorSet > texture_descriptors; //allocated from texture_descriptor_pool In the code we just wrote

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

	A1ObjectsPipeline::World world;

	struct ObjectInstance {
		SceneManager::ObjectRange object_ranges;
		A1ObjectsPipeline::Transform transform;
		size_t texture = 0;
	};
	std::vector< ObjectInstance > object_instances;

	//--------------------------------------------------------------------
	//Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
};
