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
#include "A2LambertianPipeline.hpp"
#include "A2PBRPipeline.hpp"
#include "A2ReflectionPipeline.hpp"
#include "A2CommonData.hpp"
#include "SceneManager.hpp"
#include "TextureManager.hpp"
#include "FrameBufferManager.hpp"
#include "VK.hpp"
#include "SceneTree.hpp"
#include "QueryPoolManager.hpp"

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
	// Pipelines used in this application:

	A2BackgroundPipeline background_pipeline;
	A2LambertianPipeline lambertian_pipeline;
	A2PBRPipeline pbr_pipeline;
	A2ReflectionPipeline reflection_pipeline;

	//-------------------------------------------------------------------
	//static scene resources:

	SceneManager scene_manager;
	TextureManager texture_manager;

	A2CommonData::Light global_light;

	//--------------------------------------------------------------------
	//Resources that change when the swapchain is resized:

	virtual void on_swapchain(RTG &, RTG::SwapchainEvent const &) override;
	
	FrameBufferManager framebuffer_manager;
	//--------------------------------------------------------------------
	//Resources that change when time passes or the user interacts:

	virtual void update(float dt) override;
	virtual void on_input(InputEvent const &) override;

	float time = 0.0f;

	A2CommonData::PV pv_matrix;
	A2CommonData::Light light;

	QueryPoolManager query_pool_manager;
	uint64_t gpu_frame_counter = 0;
	double last_gpu_frame_ms = 0.0;

	struct ReflectionInstance {
		S72Loader::Mesh::ObjectRange object_ranges;
		A2CommonData::Transform object_transform;
		size_t material_index;
	};
	std::vector< ReflectionInstance > reflection_object_instances;

	struct LambertianInstance {
		S72Loader::Mesh::ObjectRange object_ranges;
		A2CommonData::Transform object_transform;
		size_t material_index;
	};
	std::vector< LambertianInstance > lambertian_object_instances;

	struct PBRInstance {
		S72Loader::Mesh::ObjectRange object_ranges;
		A2CommonData::Transform object_transform;
		size_t material_index;
	};
	std::vector< PBRInstance > pbr_object_instances;
	
	std::vector< SceneTree::MeshTreeData > mesh_tree_data;
	std::vector< SceneTree::LightTreeData > light_tree_data;
	std::vector< SceneTree::CameraTreeData > camera_tree_data;
	std::vector< SceneTree::EnvironmentTreeData > environment_tree_data;

	//--------------------------------------------------------------------
	//Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
};
