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
#include "A3BackgroundPipeline.hpp"
#include "A3LambertianPipeline.hpp"
#include "A3PBRPipeline.hpp"
#include "A3ReflectionPipeline.hpp"
#include "A3ToneMappingPipeline.hpp"
#include "A3CommonData.hpp"
#include "SceneManager.hpp"
#include "TextureManager.hpp"
#include "FrameBufferManager.hpp"
#include "VK.hpp"
#include "SceneTree.hpp"
#include "QueryPoolManager.hpp"

#include "RTG.hpp"

#include <GLFW/glfw3.h>

struct A3 : RTG::Application {

	A3(RTG &);
	A3(RTG &, const std::string &);
	A3(A3 const &) = delete; //you shouldn't be copying this object
	~A3();

	//kept for use in destructor:
	RTG &rtg;
	std::shared_ptr<S72Loader::Document> doc;
	CameraManager camera_manager;
	WorkspaceManager workspace_manager;
	RenderPassManager render_pass_manager;

	//--------------------------------------------------------------------
	// Pipelines used in this application:

	A3BackgroundPipeline background_pipeline;
	A3LambertianPipeline lambertian_pipeline;
	A3PBRPipeline pbr_pipeline;
	A3ReflectionPipeline reflection_pipeline;
	A3ToneMappingPipeline tonemapping_pipeline;

	//-------------------------------------------------------------------
	//static scene resources:

	SceneManager scene_manager;
	TextureManager texture_manager;

	A3CommonData::Light global_light;

	//--------------------------------------------------------------------
	//Resources that change when the swapchain is resized:

	virtual void on_swapchain(RTG &, RTG::SwapchainEvent const &) override;
	
	FrameBufferManager framebuffer_manager;
	//--------------------------------------------------------------------
	//Resources that change when time passes or the user interacts:

	virtual void update(float dt) override;
	virtual void on_input(InputEvent const &) override;

	float time = 0.0f;

	A3CommonData::PV pv_matrix;
	A3CommonData::Light light;

	QueryPoolManager query_pool_manager;
	uint64_t gpu_frame_counter = 0;
	double last_gpu_frame_ms = 0.0;

	struct ReflectionInstance {
		S72Loader::Mesh::ObjectRange object_ranges;
		A3CommonData::Transform object_transform;
		size_t material_index;
	};
	std::vector< ReflectionInstance > reflection_object_instances;

	struct LambertianInstance {
		S72Loader::Mesh::ObjectRange object_ranges;
		A3CommonData::Transform object_transform;
		size_t material_index;
	};
	std::vector< LambertianInstance > lambertian_object_instances;

	struct PBRInstance {
		S72Loader::Mesh::ObjectRange object_ranges;
		A3CommonData::Transform object_transform;
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
