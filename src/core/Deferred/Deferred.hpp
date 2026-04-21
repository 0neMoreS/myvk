#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cstdint>
#include <cmath>
#include <vector>

#include "S72Loader.hpp"
#include "Texture2DLoader.hpp"
#include "CameraManager.hpp"
#include "WorkspaceManager.hpp"
#include "RenderPassManager.hpp"
#include "DeferredBackgroundPipeline.hpp"
#include "DeferredWritePipeline.hpp"
#include "buffer/GBufferManager.hpp"
#include "DeferredPBRPipeline.hpp"
#include "DeferredSunShadowPipeline.hpp"
#include "DeferredSpotShadowPipeline.hpp"
#include "DeferredSphereShadowPipeline.hpp"
#include "DeferredTiledLightingComputePipeline.hpp"
#include "DeferredToneMappingPipeline.hpp"
#include "DeferredCommonData.hpp"
#include "SceneManager.hpp"
#include "TextureManager.hpp"
#include "buffer/HDRBufferManager.hpp"
#include "buffer/ShadowBufferManager.hpp"
#include "LightsManager.hpp"
#include "VK.hpp"
#include "SceneTree.hpp"
#include "QueryPoolManager.hpp"

#include "RTG.hpp"

#include <GLFW/glfw3.h>

struct Deferred : RTG::Application {

	Deferred(RTG &);
	Deferred(RTG &, const std::string &);
	Deferred(Deferred const &) = delete; //you shouldn't be copying this object
	~Deferred();

	//kept for use in destructor:
	RTG &rtg;
	std::shared_ptr<S72Loader::Document> doc;
	CameraManager camera_manager;
	WorkspaceManager workspace_manager;
	RenderPassManager render_pass_manager;

	//--------------------------------------------------------------------
	// Pipelines used in this application:

	DeferredBackgroundPipeline background_pipeline;
	DeferredWritePipeline deferred_write_pipeline;
	GBufferManager gbuffer_manager;
	DeferredPBRPipeline pbr_pipeline;
	DeferredSunShadowPipeline sun_shadow_pipeline;
	DeferredSpotShadowPipeline spot_shadow_pipeline;
	DeferredSphereShadowPipeline sphere_shadow_pipeline;
	DeferredTiledLightingComputePipeline tiled_compute_pipeline;
	DeferredToneMappingPipeline tonemapping_pipeline;

	//-------------------------------------------------------------------
	//static scene resources:

	SceneManager scene_manager;
	TextureManager texture_manager;
	LightsManager lights_manager;

	//--------------------------------------------------------------------
	//Resources that change when the swapchain is resized:

	virtual void on_swapchain(RTG &, RTG::SwapchainEvent const &) override;
	
	HDRBufferManager hdrbuffer_manager;
	ShadowBufferManager shadow_buffer_manager;
	//--------------------------------------------------------------------
	//Resources that change when time passes or the user interacts:

	virtual void update(float dt) override;
	virtual void on_input(InputEvent const &) override;

	float time = 0.0f;
	static constexpr uint32_t kAOKernelSize = 64;
	std::array<glm::vec4, kAOKernelSize> ao_kernel_samples{};

	QueryPoolManager query_pool_manager;
	uint64_t gpu_frame_counter = 0;
	double last_gpu_frame_ms = 0.0;

	struct DeferredInstance {
		S72Loader::Mesh::ObjectRange object_ranges;
		DeferredCommonData::Transform object_transform;
		size_t material_index;
	};
	std::vector< DeferredInstance > deferred_object_instances;

	struct PBRInstance {
		S72Loader::Mesh::ObjectRange object_ranges;
		DeferredCommonData::Transform object_transform;
	};
	std::vector< PBRInstance > pbr_object_instances;

	struct ShadowInstance {
		S72Loader::Mesh::ObjectRange object_ranges;
		DeferredCommonData::Transform object_transform;
	};
	std::vector< ShadowInstance > shadow_object_instances;
	
	std::vector< SceneTree::MeshTreeData > mesh_tree_data;
	std::vector< SceneTree::LightTreeData > light_tree_data;
	std::vector< SceneTree::CameraTreeData > camera_tree_data;
	std::vector< SceneTree::EnvironmentTreeData > environment_tree_data;

	//--------------------------------------------------------------------
	//Rendering function, uses all the resources above to queue work to draw a frame:

	virtual void render(RTG &, RTG::RenderParams const &) override;
};
