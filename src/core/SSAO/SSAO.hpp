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
#include "SSAOBackgroundPipeline.hpp"
#include "SSAODeferredWritePipeline.hpp"
#include "SSAOAmbientOcclusionPipeline.hpp"
#include "SSAOAOBlurPipeline.hpp"
#include "buffer/GBufferManager.hpp"
#include "SSAOPBRPipeline.hpp"
#include "SSAOSunShadowPipeline.hpp"
#include "SSAOSpotShadowPipeline.hpp"
#include "SSAOSphereShadowPipeline.hpp"
#include "SSAOTiledLightingComputePipeline.hpp"
#include "SSAOToneMappingPipeline.hpp"
#include "SSAOCommonData.hpp"
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

struct SSAO : RTG::Application {

	SSAO(RTG &);
	SSAO(RTG &, const std::string &);
	SSAO(SSAO const &) = delete; //you shouldn't be copying this object
	~SSAO();

	//kept for use in destructor:
	RTG &rtg;
	std::shared_ptr<S72Loader::Document> doc;
	CameraManager camera_manager;
	WorkspaceManager workspace_manager;
	RenderPassManager render_pass_manager;

	//--------------------------------------------------------------------
	// Pipelines used in this application:

	SSAOBackgroundPipeline background_pipeline;
	SSAODeferredWritePipeline deferred_write_pipeline;
	SSAOAmbientOcclusionPipeline ao_pipeline;
	SSAOAOBlurPipeline ao_blur_pipeline;
	GBufferManager gbuffer_manager;
	SSAOPBRPipeline pbr_pipeline;
	SSAOSunShadowPipeline sun_shadow_pipeline;
	SSAOSpotShadowPipeline spot_shadow_pipeline;
	SSAOSphereShadowPipeline sphere_shadow_pipeline;
	SSAOTiledLightingComputePipeline tiled_compute_pipeline;
	SSAOToneMappingPipeline tonemapping_pipeline;

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
		SSAOCommonData::Transform object_transform;
		size_t material_index;
	};
	std::vector< DeferredInstance > deferred_object_instances;

	struct PBRInstance {
		S72Loader::Mesh::ObjectRange object_ranges;
		SSAOCommonData::Transform object_transform;
	};
	std::vector< PBRInstance > pbr_object_instances;

	struct ShadowInstance {
		S72Loader::Mesh::ObjectRange object_ranges;
		SSAOCommonData::Transform object_transform;
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
