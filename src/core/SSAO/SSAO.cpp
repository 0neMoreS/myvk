#ifdef _WIN32
//ensure we have M_PI
#define _USE_MATH_DEFINES
#endif

#include "SSAO.hpp"




#include <GLFW/glfw3.h>

#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <type_traits>

SSAO::SSAO(RTG &rtg) : SSAO(rtg, "origin-check.s72") {
}

SSAO::SSAO(RTG &rtg, const std::string &filename) : 
	rtg{rtg}, 
	doc{S72Loader::load_file(s72_dir + filename)}, 
	render_pass_manager{},
	texture_manager{}, 
	background_pipeline{},
	deferred_write_pipeline{},
	pbr_pipeline{},
	sun_shadow_pipeline{},
	spot_shadow_pipeline{},
	sphere_shadow_pipeline{},
	tiled_compute_pipeline{},
	workspace_manager{}, 
	scene_manager{},
	camera_manager{},
	framebuffer_manager{},
	mesh_tree_data{},
	light_tree_data{},
	camera_tree_data{},
	environment_tree_data{}
{
	SceneTree::traverse_scene(doc, mesh_tree_data, light_tree_data, camera_tree_data, environment_tree_data);

	camera_manager.create(doc, rtg.swapchain_extent.width, rtg.swapchain_extent.height, this->camera_tree_data, rtg.configuration);

	render_pass_manager.create(rtg, camera_manager.get_aspect_ratio(rtg.swapchain_extent, rtg.configuration.open_debug_camera));

	query_pool_manager.create(rtg, static_cast<uint32_t>(rtg.workspaces.size()));

	lights_manager.create(doc, light_tree_data, rtg.swapchain_extent);

	shadow_map_manager.create(
		rtg,
		render_pass_manager,
		lights_manager.get_shadow_sun_lights(),
		lights_manager.get_shadow_sphere_lights(),
		lights_manager.get_shadow_spot_lights()
	);

	texture_manager.create(rtg, doc, 6, static_cast<uint32_t>(lights_manager.get_shadow_sun_lights().size()), static_cast<uint32_t>(lights_manager.get_shadow_sphere_lights().size()), static_cast<uint32_t>(lights_manager.get_shadow_spot_lights().size()));

	Pipeline::ManagerContext pipeline_context{
		.texture_manager = &texture_manager,
		.shadow_map_manager = &shadow_map_manager,
	};

	// Scene pipelines render to HDR framebuffer
	background_pipeline.create(rtg, render_pass_manager.hdr_render_pass, 0, pipeline_context);

	deferred_write_pipeline.create(rtg, render_pass_manager.gbuffer_render_pass, 0, pipeline_context);

	pbr_pipeline.create(rtg, render_pass_manager.hdr_render_pass, 0, pipeline_context);

	sun_shadow_pipeline.create(rtg, render_pass_manager.spot_shadow_render_pass, 0, pipeline_context);

	spot_shadow_pipeline.create(rtg, render_pass_manager.spot_shadow_render_pass, 0, pipeline_context);

	sphere_shadow_pipeline.create(rtg, render_pass_manager.spot_shadow_render_pass, 0, pipeline_context);

	tiled_compute_pipeline.create(rtg, VK_NULL_HANDLE, 0, pipeline_context);

	// Tone mapping pipeline renders to swapchain
	tonemapping_pipeline.create(rtg, render_pass_manager.tonemap_render_pass, 0, pipeline_context);

	std::vector< std::vector< Pipeline::BlockDescriptorConfig > > block_descriptor_configs_by_pipeline{7};
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["SSAOBackgroundPipeline"]] = background_pipeline.block_descriptor_configs;
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["SSAODeferredWritePipeline"]] = deferred_write_pipeline.block_descriptor_configs;
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["SSAOPBRPipeline"]] = pbr_pipeline.block_descriptor_configs;
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["SSAOSunShadowPipeline"]] = sun_shadow_pipeline.block_descriptor_configs;
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["SSAOSpotShadowPipeline"]] = spot_shadow_pipeline.block_descriptor_configs;
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["SSAOSphereShadowPipeline"]] = sphere_shadow_pipeline.block_descriptor_configs;
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["SSAOTiledLightingComputePipeline"]] = tiled_compute_pipeline.block_descriptor_configs;

	// const uint32_t max_light_instances = static_cast<uint32_t>(light_tree_data.empty() ? 1 : light_tree_data.size());
	VkDeviceSize sun_lights_buffer_capacity = lights_manager.get_sun_lights_buffer_capacity();
	VkDeviceSize sphere_lights_buffer_capacity = lights_manager.get_sphere_lights_buffer_capacity();
	VkDeviceSize spot_lights_buffer_capacity = lights_manager.get_spot_lights_buffer_capacity();
	VkDeviceSize shadow_sun_lights_buffer_capacity = lights_manager.get_shadow_sun_lights_buffer_capacity();
	VkDeviceSize shadow_sphere_lights_buffer_capacity = lights_manager.get_shadow_sphere_lights_buffer_capacity();
	VkDeviceSize shadow_sphere_matrices_buffer_capacity = lights_manager.get_shadow_sphere_matrices_buffer_capacity();
	VkDeviceSize shadow_spot_lights_buffer_capacity = lights_manager.get_shadow_spot_lights_buffer_capacity();
	VkDeviceSize sphere_tile_data_buffer_capacity = lights_manager.get_sphere_tile_data_buffer_capacity();
	VkDeviceSize sphere_light_idx_buffer_capacity = lights_manager.get_sphere_light_idx_buffer_capacity();
	VkDeviceSize spot_tile_data_buffer_capacity = lights_manager.get_spot_tile_data_buffer_capacity();
	VkDeviceSize spot_light_idx_buffer_capacity = lights_manager.get_spot_light_idx_buffer_capacity();
	VkDeviceSize shadow_sphere_tile_data_buffer_capacity = lights_manager.get_shadow_sphere_tile_data_buffer_capacity();
	VkDeviceSize shadow_sphere_light_idx_buffer_capacity = lights_manager.get_shadow_sphere_light_idx_buffer_capacity();
	VkDeviceSize shadow_spot_tile_data_buffer_capacity = lights_manager.get_shadow_spot_tile_data_buffer_capacity();
	VkDeviceSize shadow_spot_light_idx_buffer_capacity = lights_manager.get_shadow_spot_light_idx_buffer_capacity();

	std::vector< WorkspaceManager::GlobalBufferConfig > global_buffer_configs{
		WorkspaceManager::GlobalBufferConfig{
			.name = "PV",
			.size = sizeof(SSAOCommonData::PV),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "SunLights",
			.size = sun_lights_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "SphereLights",
			.size = sphere_lights_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "SpotLights",
			.size = spot_lights_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "ShadowSunLights",
			.size = shadow_sun_lights_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "ShadowSphereLights",
			.size = shadow_sphere_lights_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "ShadowSphereMatrices",
			.size = shadow_sphere_matrices_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "ShadowSpotLights",
			.size = shadow_spot_lights_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "SphereTileData",
			.size = sphere_tile_data_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "SphereLightIdx",
			.size = sphere_light_idx_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "SpotTileData",
			.size = spot_tile_data_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "SpotLightIdx",
			.size = spot_light_idx_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "ShadowSphereTileData",
			.size = shadow_sphere_tile_data_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "ShadowSphereLightIdx",
			.size = shadow_sphere_light_idx_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "ShadowSpotTileData",
			.size = shadow_spot_tile_data_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "ShadowSpotLightIdx",
			.size = shadow_spot_light_idx_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
	};

	workspace_manager.create(rtg, std::move(block_descriptor_configs_by_pipeline), std::move(global_buffer_configs), {}, 2);
	auto update_pipeline_descriptors = [&](const char *pipeline_name, auto &pipeline, const char *descriptor_set_name, const std::vector<const char *> &binding_names) {
		for (const char *binding_name : binding_names) {
			workspace_manager.update_all_global_descriptors(
				rtg,
				pipeline_name_to_index[pipeline_name],
				pipeline.block_descriptor_set_name_to_index[descriptor_set_name],
				pipeline.block_binding_name_to_index[binding_name],
				binding_name
			);
		}
	};

	const std::vector<const char *> lit_global_bindings{
		"PV",
		"SunLights",
		"SphereLights",
		"SpotLights",
		"ShadowSunLights",
		"ShadowSphereLights",
		"ShadowSpotLights",
	};

	const std::vector<const char *> tiled_light_bindings{
		"SphereTileData",
		"SphereLightIdx",
		"SpotTileData",
		"SpotLightIdx",
		"ShadowSphereTileData",
		"ShadowSphereLightIdx",
		"ShadowSpotTileData",
		"ShadowSpotLightIdx",
	};

	std::vector<const char *> pbr_bindings = lit_global_bindings;

	pbr_bindings.insert(pbr_bindings.end(), tiled_light_bindings.begin(), tiled_light_bindings.end());

	update_pipeline_descriptors("SSAOBackgroundPipeline", background_pipeline, "PV", {"PV"});
	update_pipeline_descriptors("SSAODeferredWritePipeline", deferred_write_pipeline, "PV", {"PV"});
	update_pipeline_descriptors("SSAOPBRPipeline", pbr_pipeline, "Global", pbr_bindings);
	update_pipeline_descriptors("SSAOSunShadowPipeline", sun_shadow_pipeline, "Global", {"ShadowSunLights"});
	update_pipeline_descriptors("SSAOSpotShadowPipeline", spot_shadow_pipeline, "Global", {"ShadowSpotLights"});
	update_pipeline_descriptors("SSAOSphereShadowPipeline", sphere_shadow_pipeline, "Global", {"ShadowSphereLights", "ShadowSphereMatrices"});

	std::vector<const char *> compute_bindings = lit_global_bindings;
	compute_bindings.insert(compute_bindings.end(), tiled_light_bindings.begin(), tiled_light_bindings.end());
	update_pipeline_descriptors("SSAOTiledLightingComputePipeline", tiled_compute_pipeline, "Global", compute_bindings);

	scene_manager.create(rtg, doc);
}

SSAO::~SSAO() {
	//just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) {
		std::cerr << "Failed to vkDeviceWaitIdle in SSAO::~SSAO [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	texture_manager.destroy(rtg);

	scene_manager.destroy(rtg);

	framebuffer_manager.destroy(rtg);
	shadow_map_manager.destroy(rtg);

	background_pipeline.destroy(rtg);

	deferred_write_pipeline.destroy(rtg);

	pbr_pipeline.destroy(rtg);

	sun_shadow_pipeline.destroy(rtg);

	spot_shadow_pipeline.destroy(rtg);

	sphere_shadow_pipeline.destroy(rtg);

	tonemapping_pipeline.destroy(rtg);

    tiled_compute_pipeline.destroy(rtg);

	workspace_manager.destroy(rtg);

	render_pass_manager.destroy(rtg);

	query_pool_manager.destroy(rtg);
}

void SSAO::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain) {
	render_pass_manager.update_scissor_and_viewport(rtg_, swapchain.extent, camera_manager.get_aspect_ratio(swapchain.extent, rtg.configuration.open_debug_camera) );
	framebuffer_manager.create(rtg_, swapchain, render_pass_manager, true);

	{
		pbr_pipeline.update_gbuffer_descriptors(
			rtg_.device,
			framebuffer_manager.gbuffer_sampler,
			framebuffer_manager.gbuffer_position_depth_view,
			framebuffer_manager.gbuffer_normal_view,
			framebuffer_manager.gbuffer_albedo_view,
			framebuffer_manager.gbuffer_pbr_view
		);

		// Update descriptor to bind new HDR color image (every swapchain resize)
		VkDescriptorImageInfo image_info{
			.sampler = framebuffer_manager.hdr_sampler,
			.imageView = framebuffer_manager.hdr_color_image_view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		VkWriteDescriptorSet write{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = tonemapping_pipeline.set0_HDRTexture_instance,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &image_info,
		};

		vkUpdateDescriptorSets(rtg_.device, 1, &write, 0, nullptr);
	}
}


void SSAO::render(RTG &rtg_, RTG::RenderParams const &render_params) {
	//assert that parameters are valid:
	assert(&rtg == &rtg_);
	assert(render_params.workspace_index < workspace_manager.workspaces.size());
	assert(render_params.image_index < framebuffer_manager.swapchain_framebuffers.size());

	//get more convenient names for the current workspace and target framebuffer:
	WorkspaceManager::Workspace &workspace = workspace_manager.workspaces[render_params.workspace_index];
	VkFramebuffer framebuffer = framebuffer_manager.swapchain_framebuffers[render_params.image_index];
	//record (into `workspace.command_buffer`) commands that run a `render_pass` that just clears `framebuffer`:
	workspace.reset_recording();
	
	{ //begin recording:
		workspace.begin_recording();

		query_pool_manager.begin_frame(workspace.command_buffer, render_params.workspace_index);

		{ //upload global data:
			auto const &pv_matrix = camera_manager.get_camera_pv();

			assert(workspace.global_buffer_pairs["PV"]->host.size == sizeof(SSAOCommonData::PV));
			workspace.write_global_buffer(rtg, "PV", (void *)(&pv_matrix), sizeof(SSAOCommonData::PV));
			assert(workspace.global_buffer_pairs["PV"]->host.size == workspace.global_buffer_pairs["PV"]->device.size);

			auto const &sun_lights_bytes = lights_manager.get_sun_lights_bytes();
			auto const &sphere_lights_bytes = lights_manager.get_sphere_lights_bytes();
			auto const &spot_lights_bytes = lights_manager.get_spot_lights_bytes();
			auto const &shadow_sun_lights_bytes = lights_manager.get_shadow_sun_lights_bytes();
			auto const &shadow_sphere_lights_bytes = lights_manager.get_shadow_sphere_lights_bytes();
			auto const &shadow_sphere_matrices_bytes = lights_manager.get_shadow_sphere_matrices_bytes();
			auto const &shadow_spot_lights_bytes = lights_manager.get_shadow_spot_lights_bytes();

			assert(workspace.global_buffer_pairs["SunLights"]->host.size >= sun_lights_bytes.size());
			workspace.write_global_buffer(rtg, "SunLights", (void*)sun_lights_bytes.data(), sun_lights_bytes.size());
			assert(workspace.global_buffer_pairs["SunLights"]->host.size == workspace.global_buffer_pairs["SunLights"]->device.size);

			assert(workspace.global_buffer_pairs["SphereLights"]->host.size >= sphere_lights_bytes.size());
			workspace.write_global_buffer(rtg, "SphereLights", (void*)sphere_lights_bytes.data(), sphere_lights_bytes.size());
			assert(workspace.global_buffer_pairs["SphereLights"]->host.size == workspace.global_buffer_pairs["SphereLights"]->device.size);

			assert(workspace.global_buffer_pairs["SpotLights"]->host.size >= spot_lights_bytes.size());
			workspace.write_global_buffer(rtg, "SpotLights", (void*)spot_lights_bytes.data(), spot_lights_bytes.size());
			assert(workspace.global_buffer_pairs["SpotLights"]->host.size == workspace.global_buffer_pairs["SpotLights"]->device.size);

			assert(workspace.global_buffer_pairs["ShadowSunLights"]->host.size >= shadow_sun_lights_bytes.size());
			workspace.write_global_buffer(rtg, "ShadowSunLights", (void*)shadow_sun_lights_bytes.data(), shadow_sun_lights_bytes.size());
			assert(workspace.global_buffer_pairs["ShadowSunLights"]->host.size == workspace.global_buffer_pairs["ShadowSunLights"]->device.size);

			assert(workspace.global_buffer_pairs["ShadowSphereLights"]->host.size >= shadow_sphere_lights_bytes.size());
			workspace.write_global_buffer(rtg, "ShadowSphereLights", (void*)shadow_sphere_lights_bytes.data(), shadow_sphere_lights_bytes.size());
			assert(workspace.global_buffer_pairs["ShadowSphereLights"]->host.size == workspace.global_buffer_pairs["ShadowSphereLights"]->device.size);

			assert(workspace.global_buffer_pairs["ShadowSphereMatrices"]->host.size >= shadow_sphere_matrices_bytes.size());
			workspace.write_global_buffer(rtg, "ShadowSphereMatrices", (void*)shadow_sphere_matrices_bytes.data(), shadow_sphere_matrices_bytes.size());
			assert(workspace.global_buffer_pairs["ShadowSphereMatrices"]->host.size == workspace.global_buffer_pairs["ShadowSphereMatrices"]->device.size);

			assert(workspace.global_buffer_pairs["ShadowSpotLights"]->host.size >= shadow_spot_lights_bytes.size());
			workspace.write_global_buffer(rtg, "ShadowSpotLights", (void*)shadow_spot_lights_bytes.data(), shadow_spot_lights_bytes.size());
			assert(workspace.global_buffer_pairs["ShadowSpotLights"]->host.size == workspace.global_buffer_pairs["ShadowSpotLights"]->device.size);

			// Tile data will be generated by compute shader, so we skip host writes.
		}

		{ //upload transforms for all pipelines
			auto upload_transforms = [&](const char* pipeline_name, auto& instances, const auto& pipeline) {
				if (instances.empty()) return;
				
				size_t needed_bytes = instances.size() * sizeof(SSAOCommonData::Transform);
				uint32_t pipeline_idx = pipeline_name_to_index[pipeline_name];
				uint32_t set_idx = pipeline.block_descriptor_set_name_to_index.at("Transforms");
				uint32_t binding_idx = pipeline.block_binding_name_to_index.at("Transforms");

				auto& buffer_pair = workspace.pipeline_descriptor_set_groups[pipeline_idx][set_idx].buffer_pairs[binding_idx];
				if (buffer_pair->host.handle == VK_NULL_HANDLE || buffer_pair->host.size < needed_bytes) {
					//round to next multiple of 4k to avoid re-allocating continuously if vertex count grows slowly:
					size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
					workspace.update_descriptor(rtg, pipeline_idx, set_idx, binding_idx, new_bytes);
				}

				assert(buffer_pair->host.size == buffer_pair->device.size);
				assert(buffer_pair->host.size >= needed_bytes);
				assert(buffer_pair->host.allocation.mapped);

				std::vector<SSAOCommonData::Transform> transform_data;
				transform_data.reserve(instances.size());
				for (const auto& inst : instances) {
					transform_data.push_back(inst.object_transform);
				}
				
				workspace.write_buffer(rtg, pipeline_idx, set_idx, binding_idx, transform_data.data(), needed_bytes);
			};

			upload_transforms("SSAODeferredWritePipeline", deferred_object_instances, deferred_write_pipeline);
			upload_transforms("SSAOSunShadowPipeline", shadow_object_instances, sun_shadow_pipeline);
			upload_transforms("SSAOSpotShadowPipeline", shadow_object_instances, spot_shadow_pipeline);
			upload_transforms("SSAOSphereShadowPipeline", shadow_object_instances, sphere_shadow_pipeline);
		}

		{ //memory barrier to make sure copies complete before rendering happens:
			VkMemoryBarrier memory_barrier{
				.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
			};

			vkCmdPipelineBarrier( workspace.command_buffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, //srcStageMask
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, //dstStageMask
				0, //dependencyFlags
				1, &memory_barrier, //memoryBarriers (count, data)
				0, nullptr, //bufferMemoryBarriers (count, data)
				0, nullptr //imageMemoryBarriers (count, data)
			);
		}

		// =====================================================================
		// Compute pass to generate tiled light indices
		// =====================================================================
		{
			vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, tiled_compute_pipeline.pipeline);

			auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["SSAOTiledLightingComputePipeline"]][tiled_compute_pipeline.block_descriptor_set_name_to_index["Global"]].descriptor_set;

			vkCmdBindDescriptorSets(
				workspace.command_buffer,
				VK_PIPELINE_BIND_POINT_COMPUTE,
				tiled_compute_pipeline.layout,
				0,
				1, &global_descriptor_set,
				0, nullptr
			);

			uint32_t tiles_x = (rtg.swapchain_extent.width + 16 - 1) / 16;
			uint32_t tiles_y = (rtg.swapchain_extent.height + 16 - 1) / 16;
			
			SSAOTiledLightingComputePipeline::Push push{
				.render_width = rtg.swapchain_extent.width,
				.render_height = rtg.swapchain_extent.height,
				.tiles_x = tiles_x,
				.tiles_y = tiles_y,
			};
			vkCmdPushConstants(workspace.command_buffer, tiled_compute_pipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);

			vkCmdDispatch(workspace.command_buffer, tiles_x, tiles_y, 1);

			// Memory barrier to ensure compute writes are visible to fragment shader
			VkMemoryBarrier compute_barrier{
				.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			};
			vkCmdPipelineBarrier(workspace.command_buffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				1, &compute_barrier,
				0, nullptr,
				0, nullptr
			);
		}

		// =====================================================================
		// Sun cascade shadow pass: render depth per shadow sun light and cascade
		// =====================================================================
		{
			VkClearValue shadow_clear_value{
				.depthStencil{ .depth = rtg.configuration.reverse_z ? 0.0f : 1.0f, .stencil = 0 },
			};

			const uint32_t sun_shadow_count = std::min(
				static_cast<uint32_t>(shadow_map_manager.sun_shadow_targets.size()),
				static_cast<uint32_t>(lights_manager.get_shadow_sun_lights().size())
			);

			for (uint32_t light_index = 0; light_index < sun_shadow_count; ++light_index) {
				auto const &shadow_target = shadow_map_manager.sun_shadow_targets[light_index];

				VkExtent2D shadow_extent{
					.width = shadow_target.resolution,
					.height = shadow_target.resolution,
				};

				for (uint32_t cascade_index = 0; cascade_index < ShadowMapManager::SunCascadeCount; ++cascade_index) {
					VkRenderPassBeginInfo begin_info{
						.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
						.renderPass = render_pass_manager.spot_shadow_render_pass,
						.framebuffer = shadow_target.cascade_framebuffers[cascade_index],
						.renderArea{
							.offset = {.x = 0, .y = 0},
							.extent = shadow_extent,
						},
						.clearValueCount = 1,
						.pClearValues = &shadow_clear_value,
					};

					vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
					{
						const VkRect2D shadow_scissor = render_pass_manager.get_shadow_scissor(shadow_target.resolution);
						const VkViewport shadow_viewport = render_pass_manager.get_shadow_viewport(shadow_target.resolution);
						vkCmdSetScissor(workspace.command_buffer, 0, 1, &shadow_scissor);
						vkCmdSetViewport(workspace.command_buffer, 0, 1, &shadow_viewport);

						if (!shadow_object_instances.empty()) {
							vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sun_shadow_pipeline.pipeline);

							std::array< VkBuffer, 1 > vertex_buffers{ scene_manager.vertex_buffer.handle };
							std::array< VkDeviceSize, 1 > offsets{ 0 };
							vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());

							auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["SSAOSunShadowPipeline"]][sun_shadow_pipeline.block_descriptor_set_name_to_index["Global"]].descriptor_set;
							auto &transform_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["SSAOSunShadowPipeline"]][sun_shadow_pipeline.block_descriptor_set_name_to_index["Transforms"]].descriptor_set;

							std::array< VkDescriptorSet, 2 > descriptor_sets{
								global_descriptor_set,
								transform_descriptor_set,
							};

							vkCmdBindDescriptorSets(
								workspace.command_buffer,
								VK_PIPELINE_BIND_POINT_GRAPHICS,
								sun_shadow_pipeline.layout,
								0,
								uint32_t(descriptor_sets.size()), descriptor_sets.data(),
								0, nullptr
							);

							SSAOSunShadowPipeline::Push push{
								.LIGHT_INDEX = light_index,
								.CASCADE_INDEX = cascade_index,
							};
							vkCmdPushConstants(workspace.command_buffer, sun_shadow_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);

							for (uint32_t i = 0; i < shadow_object_instances.size(); ++i) {
								vkCmdDraw(workspace.command_buffer, shadow_object_instances[i].object_ranges.count, 1, shadow_object_instances[i].object_ranges.first, i);
							}
						}
					}
					vkCmdEndRenderPass(workspace.command_buffer);
				}
			}
		}

		// =====================================================================
		// Sphere shadow pass: render depth cubemap for each shadow-casting sphere light
		// =====================================================================
		{
			VkClearValue shadow_clear_value{
				.depthStencil{ .depth = rtg.configuration.reverse_z ? 0.0f : 1.0f, .stencil = 0 },
			};

			const uint32_t sphere_shadow_count = std::min(
				static_cast<uint32_t>(shadow_map_manager.sphere_shadow_targets.size()),
				static_cast<uint32_t>(lights_manager.get_shadow_sphere_lights().size())
			);

			for (uint32_t light_index = 0; light_index < sphere_shadow_count; ++light_index) {
				auto const &shadow_target = shadow_map_manager.sphere_shadow_targets[light_index];

				VkExtent2D shadow_extent{
					.width = shadow_target.resolution,
					.height = shadow_target.resolution,
				};

				for (uint32_t face_index = 0; face_index < ShadowMapManager::SphereFaceCount; ++face_index) {
					VkRenderPassBeginInfo begin_info{
						.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
						.renderPass = render_pass_manager.spot_shadow_render_pass,
						.framebuffer = shadow_target.face_framebuffers[face_index],
						.renderArea{
							.offset = {.x = 0, .y = 0},
							.extent = shadow_extent,
						},
						.clearValueCount = 1,
						.pClearValues = &shadow_clear_value,
					};

					vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
					{
						const VkRect2D shadow_scissor = render_pass_manager.get_shadow_scissor(shadow_target.resolution);
						const VkViewport shadow_viewport = render_pass_manager.get_shadow_viewport(shadow_target.resolution);
						vkCmdSetScissor(workspace.command_buffer, 0, 1, &shadow_scissor);
						vkCmdSetViewport(workspace.command_buffer, 0, 1, &shadow_viewport);

						if (!shadow_object_instances.empty()) {
							vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, sphere_shadow_pipeline.pipeline);

							std::array< VkBuffer, 1 > vertex_buffers{ scene_manager.vertex_buffer.handle };
							std::array< VkDeviceSize, 1 > offsets{ 0 };
							vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());

							auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["SSAOSphereShadowPipeline"]][sphere_shadow_pipeline.block_descriptor_set_name_to_index["Global"]].descriptor_set;
							auto &transform_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["SSAOSphereShadowPipeline"]][sphere_shadow_pipeline.block_descriptor_set_name_to_index["Transforms"]].descriptor_set;

							std::array< VkDescriptorSet, 2 > descriptor_sets{
								global_descriptor_set,
								transform_descriptor_set,
							};

							vkCmdBindDescriptorSets(
								workspace.command_buffer,
								VK_PIPELINE_BIND_POINT_GRAPHICS,
								sphere_shadow_pipeline.layout,
								0,
								uint32_t(descriptor_sets.size()), descriptor_sets.data(),
								0, nullptr
							);

							SSAOSphereShadowPipeline::Push push{
								.LIGHT_INDEX = light_index,
								.FACE_INDEX = face_index,
							};
							vkCmdPushConstants(workspace.command_buffer, sphere_shadow_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);

							for (uint32_t i = 0; i < shadow_object_instances.size(); ++i) {
								vkCmdDraw(workspace.command_buffer, shadow_object_instances[i].object_ranges.count, 1, shadow_object_instances[i].object_ranges.first, i);
							}
						}
					}
					vkCmdEndRenderPass(workspace.command_buffer);
				}
			}
		}

		// =====================================================================
		// Spot shadow pass: render depth for each shadow-casting spot light
		// =====================================================================
		{
			VkClearValue shadow_clear_value{
				.depthStencil{ .depth = rtg.configuration.reverse_z ? 0.0f : 1.0f, .stencil = 0 },
			};

			const uint32_t shadow_count = std::min(
				static_cast<uint32_t>(shadow_map_manager.spot_shadow_targets.size()),
				static_cast<uint32_t>(lights_manager.get_shadow_spot_lights().size())
			);

			for (uint32_t light_index = 0; light_index < shadow_count; ++light_index) {
				auto const &shadow_target = shadow_map_manager.spot_shadow_targets[light_index];
				
				VkExtent2D shadow_extent{
					.width = shadow_target.resolution,
					.height = shadow_target.resolution,
				};

				VkRenderPassBeginInfo begin_info{
					.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
					.renderPass = render_pass_manager.spot_shadow_render_pass,
					.framebuffer = shadow_target.framebuffer,
					.renderArea{
						.offset = {.x = 0, .y = 0},
						.extent = shadow_extent,
					},
					.clearValueCount = 1,
					.pClearValues = &shadow_clear_value,
				};

				vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
				{
					const VkRect2D shadow_scissor = render_pass_manager.get_shadow_scissor(shadow_target.resolution);
					const VkViewport shadow_viewport = render_pass_manager.get_shadow_viewport(shadow_target.resolution);
					vkCmdSetScissor(workspace.command_buffer, 0, 1, &shadow_scissor);
					vkCmdSetViewport(workspace.command_buffer, 0, 1, &shadow_viewport);

					if (!shadow_object_instances.empty()) {
						vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spot_shadow_pipeline.pipeline);

						std::array< VkBuffer, 1 > vertex_buffers{ scene_manager.vertex_buffer.handle };
						std::array< VkDeviceSize, 1 > offsets{ 0 };
						vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());

						auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["SSAOSpotShadowPipeline"]][spot_shadow_pipeline.block_descriptor_set_name_to_index["Global"]].descriptor_set;
						auto &transform_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["SSAOSpotShadowPipeline"]][spot_shadow_pipeline.block_descriptor_set_name_to_index["Transforms"]].descriptor_set;

						std::array< VkDescriptorSet, 2 > descriptor_sets{
							global_descriptor_set,
							transform_descriptor_set,
						};

						vkCmdBindDescriptorSets(
							workspace.command_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							spot_shadow_pipeline.layout,
							0,
							uint32_t(descriptor_sets.size()), descriptor_sets.data(),
							0, nullptr
						);

						SSAOSpotShadowPipeline::Push push{
							.LIGHT_INDEX = light_index,
						};
						vkCmdPushConstants(workspace.command_buffer, spot_shadow_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);

						for (uint32_t i = 0; i < shadow_object_instances.size(); ++i) {
							vkCmdDraw(workspace.command_buffer, shadow_object_instances[i].object_ranges.count, 1, shadow_object_instances[i].object_ranges.first, i);
						}
					}
				}
				vkCmdEndRenderPass(workspace.command_buffer);
			}
		}


		// =====================================================================
		// Deferred write pass: Render scene geometry to GBuffer
		// =====================================================================
		{
			VkRenderPassBeginInfo begin_info{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = render_pass_manager.gbuffer_render_pass,
				.framebuffer = framebuffer_manager.gbuffer_framebuffer,
				.renderArea{
					.offset = {.x = 0, .y = 0},
					.extent = rtg.swapchain_extent,
				},
				.clearValueCount = uint32_t(render_pass_manager.gbuffer_clears.size()),
				.pClearValues = render_pass_manager.gbuffer_clears.data(),
			};

			vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
			{
				vkCmdSetScissor(workspace.command_buffer, 0, 1, &render_pass_manager.full_scissor);
				vkCmdSetViewport(workspace.command_buffer, 0, 1, &render_pass_manager.full_viewport);

				if (!deferred_object_instances.empty()) {
					vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferred_write_pipeline.pipeline);

					std::array< VkBuffer, 1 > vertex_buffers{ scene_manager.vertex_buffer.handle };
					std::array< VkDeviceSize, 1 > offsets{ 0 };
					vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());

					auto &pv_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["SSAODeferredWritePipeline"]][deferred_write_pipeline.block_descriptor_set_name_to_index["PV"]].descriptor_set;
					auto &transform_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["SSAODeferredWritePipeline"]][deferred_write_pipeline.block_descriptor_set_name_to_index["Transforms"]].descriptor_set;

					std::array< VkDescriptorSet, 3 > descriptor_sets{
						pv_descriptor_set,
						transform_descriptor_set,
						deferred_write_pipeline.set2_Textures_instance,
					};

					vkCmdBindDescriptorSets(
						workspace.command_buffer,
						VK_PIPELINE_BIND_POINT_GRAPHICS,
						deferred_write_pipeline.layout,
						0,
						uint32_t(descriptor_sets.size()), descriptor_sets.data(),
						0, nullptr
					);

					for (uint32_t i = 0; i < deferred_object_instances.size(); ++i) {
						SSAODeferredWritePipeline::Push push{
							.MATERIAL_INDEX = uint32_t(deferred_object_instances[i].material_index),
						};

						vkCmdPushConstants(workspace.command_buffer, deferred_write_pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
						vkCmdDraw(workspace.command_buffer, deferred_object_instances[i].object_ranges.count, 1, deferred_object_instances[i].object_ranges.first, i);
					}
				}
			}
			vkCmdEndRenderPass(workspace.command_buffer);
		}

		// =====================================================================
		// GBuffer barrier: make deferred targets visible to lighting shader reads
		// =====================================================================
		{
			std::array<VkImageMemoryBarrier, 4> gbuffer_barriers{
				VkImageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = framebuffer_manager.gbuffer_position_depth_image.handle,
					.subresourceRange{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				},
				VkImageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = framebuffer_manager.gbuffer_normal_image.handle,
					.subresourceRange{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				},
				VkImageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = framebuffer_manager.gbuffer_albedo_image.handle,
					.subresourceRange{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				},
				VkImageMemoryBarrier{
					.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
					.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
					.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
					.image = framebuffer_manager.gbuffer_pbr_image.handle,
					.subresourceRange{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
				},
			};

			vkCmdPipelineBarrier(
				workspace.command_buffer,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0, nullptr,
				0, nullptr,
				uint32_t(gbuffer_barriers.size()), gbuffer_barriers.data()
			);
		}

		// =====================================================================
		// First pass: Render scene to HDR framebuffer
		// =====================================================================
		{
			VkRenderPassBeginInfo begin_info{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = render_pass_manager.hdr_render_pass,
				.framebuffer = framebuffer_manager.hdr_framebuffer,
				.renderArea{
					.offset = {.x = 0, .y = 0},
					.extent = rtg.swapchain_extent,
				},
				.clearValueCount = uint32_t(render_pass_manager.clears.size()),
				.pClearValues = render_pass_manager.clears.data(),
			};

			vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
			{
				// run pipelines here
				{ //set scissor rectangle:
					vkCmdSetScissor(workspace.command_buffer, 0, 1, &render_pass_manager.full_scissor);
					vkCmdSetViewport(workspace.command_buffer, 0, 1, &render_pass_manager.full_viewport);
				}

				{ //draw skybox with background pipeline if available
					if (doc->environments.size() > 0) {
						vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, background_pipeline.pipeline);

						{ //use scene vertex buffer as binding 0:
							std::array< VkBuffer, 1 > vertex_buffers{ scene_manager.cubemap_vertex_buffer.handle };
							std::array< VkDeviceSize, 1 > offsets{ 0 };
							vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
						}

						{
							std::array< VkDescriptorSet, 2 > descriptor_sets{
								workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["SSAOBackgroundPipeline"]][background_pipeline.block_descriptor_set_name_to_index["PV"]].descriptor_set, //0: PV
								background_pipeline.set1_CUBEMAP_instance, //1: Cubemap
							};

							vkCmdBindDescriptorSets(
								workspace.command_buffer, //command buffer
								VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
								background_pipeline.layout, //pipeline layout
								0, //set 0
								uint32_t(descriptor_sets.size()), descriptor_sets.data(), //descriptor_set sets count, ptr
								0, nullptr //dynamic offsets count, ptr
							);
						}

						vkCmdDraw(workspace.command_buffer, 36, 1, 0, 0); // hard coded for a cube
					}
				}

				{ // deferred lighting fullscreen pass
					vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_pipeline.pipeline);

					{ // bind Global and Textures descriptor sets (Transforms stays bound for compatibility)
						auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["SSAOPBRPipeline"]][pbr_pipeline.block_descriptor_set_name_to_index["Global"]].descriptor_set;
						auto &transform_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["SSAOPBRPipeline"]][pbr_pipeline.block_descriptor_set_name_to_index["Transforms"]].descriptor_set;
						auto &textures_descriptor_set = pbr_pipeline.set2_Textures_instance;

						std::array< VkDescriptorSet, 3 > descriptor_sets{
							global_descriptor_set,
							transform_descriptor_set,
							textures_descriptor_set,
						};
						vkCmdBindDescriptorSets(
							workspace.command_buffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							pbr_pipeline.layout,
							0,
							uint32_t(descriptor_sets.size()), descriptor_sets.data(),
							0, nullptr
						);
					}

					vkCmdDraw(workspace.command_buffer, 3, 1, 0, 0);
				}
			}

			vkCmdEndRenderPass(workspace.command_buffer);
		}

		// =====================================================================
		// Image barrier: HDR texture ready for sampling
		// =====================================================================
		{
			VkImageMemoryBarrier image_barrier{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.image = framebuffer_manager.hdr_color_image.handle,
				.subresourceRange{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
			};

			vkCmdPipelineBarrier(
				workspace.command_buffer,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,        // dstStageMask
				0,                                             // dependencyFlags
				0, nullptr,                                    // memoryBarriers
				0, nullptr,                                    // bufferMemoryBarriers
				1, &image_barrier                              // imageMemoryBarriers
			);
		}

		// =====================================================================
		// Second pass: Tone mapping HDR texture to swapchain
		// =====================================================================
		{
			VkRenderPassBeginInfo begin_info{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = render_pass_manager.tonemap_render_pass,
				.framebuffer = framebuffer,
				.renderArea{
					.offset = {.x = 0, .y = 0},
					.extent = rtg.swapchain_extent,
				},
				.clearValueCount = uint32_t(render_pass_manager.tonemap_clears.size()),
				.pClearValues = render_pass_manager.tonemap_clears.data(),
			};

			vkCmdBeginRenderPass(workspace.command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
			{
				{
					vkCmdSetScissor(workspace.command_buffer, 0, 1, &render_pass_manager.scissor);
					vkCmdSetViewport(workspace.command_buffer, 0, 1, &render_pass_manager.viewport);
					vkCmdClearAttachments(workspace.command_buffer, 1, &render_pass_manager.clear_center_attachment, 1, &render_pass_manager.clear_center_rect);
				}		

				// Bind tone mapping pipeline
				vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapping_pipeline.pipeline);

				// Bind HDR texture descriptor
				std::array< VkDescriptorSet, 1 > descriptor_sets{
					tonemapping_pipeline.set0_HDRTexture_instance,
				};

				vkCmdBindDescriptorSets(
					workspace.command_buffer,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					tonemapping_pipeline.layout,
					0,
					uint32_t(descriptor_sets.size()), descriptor_sets.data(),
					0, nullptr
				);

				SSAOToneMappingPipeline::Push push{
					.EXPOSURE = rtg.configuration.tone_exposure,
					.METHOD = static_cast<uint32_t>(rtg.configuration.tone_map_method)
				};
				// Draw full-screen triangle (no vertex buffer)
				vkCmdPushConstants(workspace.command_buffer, tonemapping_pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
				vkCmdDraw(workspace.command_buffer, 3, 1, 0, 0);
			}

			vkCmdEndRenderPass(workspace.command_buffer);
		}

		query_pool_manager.end_frame(workspace.command_buffer, render_params.workspace_index);

		//end recording:
		workspace.end_recording();
	}

	{ //submit `workspace.command buffer` for the GPU to run:
		std::array< VkSemaphore, 1 > wait_semaphores{
			render_params.image_available
		};
		std::array< VkPipelineStageFlags, 1 > wait_stages{
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		};
		static_assert(wait_semaphores.size() == wait_stages.size(), "every semaphore needs a stage");

		std::array< VkSemaphore, 1 > signal_semaphores{
			render_params.image_done
		};
		VkSubmitInfo submit_info{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = uint32_t(wait_semaphores.size()),
			.pWaitSemaphores = wait_semaphores.data(),
			.pWaitDstStageMask = wait_stages.data(),
			.commandBufferCount = 1,
			.pCommandBuffers = &workspace.command_buffer,
			.signalSemaphoreCount = uint32_t(signal_semaphores.size()),
			.pSignalSemaphores = signal_semaphores.data(),
		};

		VK( vkQueueSubmit(rtg.graphics_queue, 1, &submit_info, render_params.workspace_available) );
	}

	if (rtg.configuration.headless &&  query_pool_manager.is_enabled()) {
		double frame_ms = 0.0;
        if (query_pool_manager.fetch_frame_ms(rtg, render_params.workspace_index, frame_ms)) {
            ++gpu_frame_counter;
            last_gpu_frame_ms = frame_ms;
			std::cout << "GPU Headless frame time: " << last_gpu_frame_ms << " ms" << std::endl;
        }
	}
}


void SSAO::update(float dt) {
	time = std::fmod(time + dt, 8.0f);

	SceneTree::update_animation(doc, time);
	SceneTree::traverse_scene(doc, mesh_tree_data, light_tree_data, camera_tree_data, environment_tree_data);

	{ // update global data
		camera_manager.update(dt, camera_tree_data, rtg.configuration);

		lights_manager.update(
			doc,
			light_tree_data,
			camera_manager
		);
	}

	{ // update object instances with frustum culling
		pbr_object_instances.clear();
		lambertian_object_instances.clear();
		deferred_object_instances.clear();
		shadow_object_instances.clear();
		// Get frustum for culling
		auto frustum = camera_manager.get_frustum();

		for(auto &mtd : mesh_tree_data){
			const size_t mesh_index = mtd.mesh_index;
			const size_t material_index = mtd.material_index;
			const glm::mat4 MODEL = BLENDER_TO_VULKAN_4 * mtd.model_matrix;
			const glm::mat4 MODEL_NORMAL = glm::transpose(glm::inverse(MODEL));
			const auto& object_range = doc->meshes[mesh_index].range;
			std::optional<S72Loader::Material> material = doc->materials[material_index];

			ShadowInstance shadow_inst{
				.object_ranges = object_range,
				.object_transform{
					.MODEL = MODEL,
					.MODEL_NORMAL = MODEL_NORMAL,
				},
			};
			shadow_object_instances.emplace_back(std::move(shadow_inst));

			// Transform local AABB to world AABB (8 corners method)
			const glm::vec3& bmin = object_range.aabb_min;
			const glm::vec3& bmax = object_range.aabb_max;
			glm::vec3 corners[8] = {
				{bmin.x, bmin.y, bmin.z}, {bmax.x, bmin.y, bmin.z}, {bmin.x, bmax.y, bmin.z}, {bmax.x, bmax.y, bmin.z},
				{bmin.x, bmin.y, bmax.z}, {bmax.x, bmin.y, bmax.z}, {bmin.x, bmax.y, bmax.z}, {bmax.x, bmax.y, bmax.z}
			};
			glm::vec3 world_min(std::numeric_limits<float>::max());
			glm::vec3 world_max(std::numeric_limits<float>::lowest());
			for (int c = 0; c < 8; ++c) {
				glm::vec3 wp = glm::vec3(MODEL * glm::vec4(corners[c], 1.0f));
				world_min = glm::min(world_min, wp);
				world_max = glm::max(world_max, wp);
			}

			// Frustum culling check with world-space AABB
			if (!frustum.is_box_visible(world_min, world_max)) {
				continue;
			}

			// Lambertian material instance
			if(material.has_value() && material->lambertian) {
				LambertianInstance lambertian_inst{
					.object_ranges = object_range,
					.object_transform{
						.MODEL = MODEL,
						.MODEL_NORMAL = MODEL_NORMAL,
					},
					.material_index = material_index,
				};

				lambertian_object_instances.emplace_back(std::move(lambertian_inst));

				DeferredInstance deferred_inst{
					.object_ranges = object_range,
					.object_transform{
						.MODEL = MODEL,
						.MODEL_NORMAL = MODEL_NORMAL,
					},
					.material_index = material_index,
				};
				deferred_object_instances.emplace_back(std::move(deferred_inst));
			}

			// PBR material instance
			if(material.has_value() && material->pbr) {
				PBRInstance pbr_inst{
					.object_ranges = object_range,
					.object_transform{
						.MODEL = MODEL,
						.MODEL_NORMAL = MODEL_NORMAL,
					},
					.material_index = material_index,
				};

				pbr_object_instances.emplace_back(std::move(pbr_inst));

				DeferredInstance deferred_inst{
					.object_ranges = object_range,
					.object_transform{
						.MODEL = MODEL,
						.MODEL_NORMAL = MODEL_NORMAL,
					},
					.material_index = material_index,
				};
				deferred_object_instances.emplace_back(std::move(deferred_inst));
			}
		}
	}
}


void SSAO::on_input(InputEvent const &event) {
	camera_manager.on_input(event);

	if (event.type == InputEvent::KeyDown) {
	// Change active camera with TAB
		if (event.key.key == GLFW_KEY_TAB) {
			camera_manager.change_active_camera();
			render_pass_manager.update_scissor_and_viewport(rtg, rtg.swapchain_extent, camera_manager.get_aspect_ratio(rtg.swapchain_extent, rtg.configuration.open_debug_camera) );
		}

		if (event.key.key == GLFW_KEY_LEFT_ALT){
			rtg.configuration.open_debug_camera = !rtg.configuration.open_debug_camera;
			render_pass_manager.update_scissor_and_viewport(rtg, rtg.swapchain_extent, camera_manager.get_aspect_ratio(rtg.swapchain_extent, rtg.configuration.open_debug_camera) );
		}
	}
}
