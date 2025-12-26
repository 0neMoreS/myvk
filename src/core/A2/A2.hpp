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
#include "A2BackgroundPipeline.hpp"
#include "A2EnvironmentPipeline.hpp"
#include "CommonLayouts.hpp"
#include "SceneManager.hpp"
#include "TextureManager.hpp"
#include "FrameBufferManager.hpp"
#include "VK.hpp"

#include "RTG.hpp"

#include <GLFW/glfw3.h>

struct A2 : RTG::Application {

	A2(RTG &);
	A2(RTG &, const std::string &);
	A2(A2 const &) = delete; //you shouldn't be copying this object
	~A2();

	//kept for use in destructor:
	RTG &rtg;
	std::shared_ptr<S72Loader::Document> doc;
	CameraManager camera_manager;
	WorkspaceManager workspace_manager;
	RenderPassManager render_pass_manager;

	//--------------------------------------------------------------------
	//Resources that last the lifetime of the application:

	CommonLayouts common_layouts;
	A2BackgroundPipeline background_pipeline;
	A2EnvironmentPipeline objects_pipeline;

	//-------------------------------------------------------------------
	//static scene resources:

	SceneManager scene_manager;
	TextureManager texture_manager;

	//--------------------------------------------------------------------
	//Resources that change when the swapchain is resized:

	virtual void on_swapchain(RTG &, RTG::SwapchainEvent const &) override;
	
	FrameBufferManager framebuffer_manager;
	//--------------------------------------------------------------------
	//Resources that change when time passes or the user interacts:

	virtual void update(float dt) override;
	virtual void on_input(InputEvent const &) override;

	float time = 0.0f;

	CommonLayouts::PV pv_matrix;
	A2EnvironmentPipeline::World object_world;

	struct ObjectInstance {
		SceneManager::ObjectRange object_ranges;
		A2EnvironmentPipeline::Transform object_transform;
		size_t material_index;
	};
	std::vector< ObjectInstance > object_instances;

	//--------------------------------------------------------------------
	//Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
};
