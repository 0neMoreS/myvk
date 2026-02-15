#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>

#include <cmath>

#include "S72Loader.hpp"
#include "Texture2DLoader.hpp"
#include "CameraManager.hpp"
#include "WorkspaceManager.hpp"
#include "RenderPassManager.hpp"
#include "A1LinesPipeline.hpp"
#include "A1ObjectsPipeline.hpp"
#include "SceneManager.hpp"
#include "TextureManager.hpp"
#include "FrameBufferManager.hpp"
#include "VK.hpp"
#include "SceneTree.hpp"
#include "A1CommonData.hpp"
#include "PosColVertex.hpp"
#include "QueryPoolManager.hpp"

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

	A1LinesPipeline lines_pipeline;
	A1ObjectsPipeline objects_pipeline;

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

	A1CommonData::PV pv_matrix;
	A1CommonData::World world_lighting;

	QueryPoolManager query_pool_manager;
	uint64_t gpu_frame_counter = 0;
	double last_gpu_frame_ms = 0.0;

	struct ObjectInstance {
		S72Loader::Mesh::ObjectRange object_ranges;
		A1ObjectsPipeline::Transform transform;
		size_t material_index;
	};
	std::vector< ObjectInstance > object_instances;

	std::vector< PosColVertex > line_vertices;

	std::vector< SceneTree::MeshTreeData > mesh_tree_data;
	std::vector< SceneTree::LightTreeData > light_tree_data;
	std::vector< SceneTree::CameraTreeData > camera_tree_data;
	std::vector< SceneTree::EnvironmentTreeData > environment_tree_data;

	//--------------------------------------------------------------------
	//Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
};
