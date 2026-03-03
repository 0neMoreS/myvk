#ifdef _WIN32
//ensure we have M_PI
#define _USE_MATH_DEFINES
#endif

#include "A3.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <type_traits>

A3::A3(RTG &rtg) : A3(rtg, "origin-check.s72") {
}

A3::A3(RTG &rtg, const std::string &filename) : 
	rtg{rtg}, 
	doc{S72Loader::load_file(s72_dir + filename)}, 
	render_pass_manager{},
	texture_manager{}, 
	background_pipeline{},
	lambertian_pipeline{},
	pbr_pipeline{},
	spot_shadow_pipeline{},
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

	camera_manager.create(doc, rtg.swapchain_extent.width, rtg.swapchain_extent.height, this->camera_tree_data, rtg.configuration.init_camera_name);

	render_pass_manager.create(rtg, camera_manager.get_aspect_ratio(rtg.configuration.open_debug_camera, rtg.swapchain_extent));

	query_pool_manager.create(rtg, static_cast<uint32_t>(rtg.workspaces.size()));

	sun_lights.reserve(light_tree_data.size());
	sphere_lights.reserve(light_tree_data.size());
	spot_lights.reserve(light_tree_data.size());
	shadow_sun_lights.reserve(light_tree_data.size());
	shadow_sphere_lights.reserve(light_tree_data.size());
	shadow_spot_lights.reserve(light_tree_data.size());

	for (const auto &ltd : light_tree_data) {
		if (ltd.light_index >= doc->lights.size()) continue;

		const auto &src_light = doc->lights[ltd.light_index];
		const bool has_shadow = (src_light.shadow != 0);
		const glm::mat4 model = BLENDER_TO_VULKAN_4 * ltd.model_matrix;
		const glm::vec3 position = glm::vec3(model[3]);
		const glm::vec3 direction = glm::normalize(glm::vec3(model * glm::vec4(0.0f, 0.0f, 1.0f, 0.0f)));

		if (src_light.sun) {
			A3CommonData::SunLight dst{};
			for (int i = 0; i < 4; ++i) dst.cascadeSplits[i] = 0.0f;
			for (int i = 0; i < 4; ++i) dst.orthographic[i] = glm::mat4(1.0f);
			dst.direction = direction;
			dst.angle = src_light.sun->angle;
			dst.tint = src_light.tint * src_light.sun->strength;
			dst.shadow = static_cast<int32_t>(src_light.shadow);
			if (has_shadow) shadow_sun_lights.emplace_back(std::move(dst));
			else sun_lights.emplace_back(std::move(dst));
		}

		if (src_light.sphere) {
			A3CommonData::SphereLight dst{};
			dst.position = position;
			dst.radius = src_light.sphere->radius;
			dst.tint = src_light.tint * src_light.sphere->power;
			dst.limit = src_light.sphere->limit.value_or(2.0f * std::sqrt(src_light.sphere->power / (4.0f * M_PI) * 256.0f));
			if (has_shadow) shadow_sphere_lights.emplace_back(std::move(dst));
			else sphere_lights.emplace_back(std::move(dst));
		}

		if (src_light.spot) {
			A3CommonData::SpotLight dst{};
			dst.perspective = glm::mat4(1.0f);
			dst.position = position;
			dst.radius = src_light.spot->radius;
			dst.direction = direction;
			dst.fov = src_light.spot->fov;
			dst.tint = src_light.tint * src_light.spot->power;
			dst.blend = src_light.spot->blend;
			dst.limit = src_light.spot->limit.value_or(2.0f * std::sqrt(src_light.spot->power / (4.0f * M_PI) * 256.0f));
			dst.shadow = static_cast<int32_t>(src_light.shadow);
			if (has_shadow) shadow_spot_lights.emplace_back(std::move(dst));
			else spot_lights.emplace_back(std::move(dst));
		}
	}

	shadow_map_manager.create(rtg, render_pass_manager, shadow_spot_lights);

	texture_manager.create(rtg, doc, 5, static_cast<uint32_t>(shadow_sun_lights.size()), static_cast<uint32_t>(shadow_sphere_lights.size()), static_cast<uint32_t>(shadow_spot_lights.size())); // 5 pipelines: background, lambertian, pbr, reflection, tonemapping

	// Scene pipelines render to HDR framebuffer
	background_pipeline.create(rtg, render_pass_manager.hdr_render_pass, 0, texture_manager, nullptr);

	lambertian_pipeline.create(rtg, render_pass_manager.hdr_render_pass, 0, texture_manager, &shadow_map_manager);

	pbr_pipeline.create(rtg, render_pass_manager.hdr_render_pass, 0, texture_manager, &shadow_map_manager);

	spot_shadow_pipeline.create(rtg, render_pass_manager.spot_shadow_render_pass, 0, texture_manager, nullptr);

	// Tone mapping pipeline renders to swapchain
	tonemapping_pipeline.create(rtg, render_pass_manager.tonemap_render_pass, 0, texture_manager, nullptr);

	std::vector< std::vector< Pipeline::BlockDescriptorConfig > > block_descriptor_configs_by_pipeline{4};
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["A3BackgroundPipeline"]] = background_pipeline.block_descriptor_configs;
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["A3LambertianPipeline"]] = lambertian_pipeline.block_descriptor_configs;
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["A3PBRPipeline"]] = pbr_pipeline.block_descriptor_configs;
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["A3SpotShadowPipeline"]] = spot_shadow_pipeline.block_descriptor_configs;

	// const uint32_t max_light_instances = static_cast<uint32_t>(light_tree_data.empty() ? 1 : light_tree_data.size());
	VkDeviceSize sun_lights_buffer_capacity = A3CommonData::sun_lights_buffer_size(static_cast<uint32_t>(sun_lights.size()));
	VkDeviceSize sphere_lights_buffer_capacity = A3CommonData::sphere_lights_buffer_size(static_cast<uint32_t>(sphere_lights.size()));
	VkDeviceSize spot_lights_buffer_capacity = A3CommonData::spot_lights_buffer_size(static_cast<uint32_t>(spot_lights.size()));
	VkDeviceSize shadow_sun_lights_buffer_capacity = A3CommonData::sun_lights_buffer_size(static_cast<uint32_t>(shadow_sun_lights.size()));
	VkDeviceSize shadow_sphere_lights_buffer_capacity = A3CommonData::sphere_lights_buffer_size(static_cast<uint32_t>(shadow_sphere_lights.size()));
	VkDeviceSize shadow_spot_lights_buffer_capacity = A3CommonData::spot_lights_buffer_size(static_cast<uint32_t>(shadow_spot_lights.size()));
	sun_lights_bytes.assign(static_cast<size_t>(sun_lights_buffer_capacity), 0);
	sphere_lights_bytes.assign(static_cast<size_t>(sphere_lights_buffer_capacity), 0);
	spot_lights_bytes.assign(static_cast<size_t>(spot_lights_buffer_capacity), 0);
	shadow_sun_lights_bytes.assign(static_cast<size_t>(shadow_sun_lights_buffer_capacity), 0);
	shadow_sphere_lights_bytes.assign(static_cast<size_t>(shadow_sphere_lights_buffer_capacity), 0);
	shadow_spot_lights_bytes.assign(static_cast<size_t>(shadow_spot_lights_buffer_capacity), 0);

	{
		auto init_lights_bytes = [](auto const &lights, std::vector<uint8_t> &bytes) {
			using LightT = typename std::decay_t<decltype(lights)>::value_type;
			const size_t header_size = sizeof(A3CommonData::LightsHeader);
			const size_t payload_size = sizeof(LightT) * lights.size();
			assert(bytes.size() == header_size + payload_size);

			A3CommonData::LightsHeader header{};
			header.count = static_cast<uint32_t>(lights.size());
			std::memcpy(bytes.data(), &header, header_size);
			if (!lights.empty()) {
				std::memcpy(bytes.data() + header_size, lights.data(), payload_size);
			}
		};

		init_lights_bytes(sun_lights, sun_lights_bytes);
		init_lights_bytes(sphere_lights, sphere_lights_bytes);
		init_lights_bytes(spot_lights, spot_lights_bytes);
		init_lights_bytes(shadow_sun_lights, shadow_sun_lights_bytes);
		init_lights_bytes(shadow_sphere_lights, shadow_sphere_lights_bytes);
		init_lights_bytes(shadow_spot_lights, shadow_spot_lights_bytes);
	}

	std::vector< WorkspaceManager::GlobalBufferConfig > global_buffer_configs{
		WorkspaceManager::GlobalBufferConfig{
			.name = "PV",
			.size = sizeof(A3CommonData::PV),
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
			.name = "ShadowSpotLights",
			.size = shadow_spot_lights_buffer_capacity,
			.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
		},
	};

	workspace_manager.create(rtg, std::move(block_descriptor_configs_by_pipeline), std::move(global_buffer_configs), {}, 2);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A3BackgroundPipeline"], 
		background_pipeline.block_descriptor_set_name_to_index["PV"], 
		background_pipeline.block_binding_name_to_index["PV"], 
		"PV"
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A3LambertianPipeline"], 
		lambertian_pipeline.block_descriptor_set_name_to_index["Global"], 
		lambertian_pipeline.block_binding_name_to_index["PV"], 
		"PV"
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A3LambertianPipeline"], 
		lambertian_pipeline.block_descriptor_set_name_to_index["Global"], 
		lambertian_pipeline.block_binding_name_to_index["SunLights"], 
		"SunLights"
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A3LambertianPipeline"], 
		lambertian_pipeline.block_descriptor_set_name_to_index["Global"], 
		lambertian_pipeline.block_binding_name_to_index["SphereLights"], 
		"SphereLights"
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A3LambertianPipeline"], 
		lambertian_pipeline.block_descriptor_set_name_to_index["Global"], 
		lambertian_pipeline.block_binding_name_to_index["SpotLights"], 
		"SpotLights"
	);
	workspace_manager.update_all_global_descriptors(
		rtg,
		pipeline_name_to_index["A3LambertianPipeline"],
		lambertian_pipeline.block_descriptor_set_name_to_index["Global"],
		lambertian_pipeline.block_binding_name_to_index["ShadowSunLights"],
		"ShadowSunLights"
	);
	workspace_manager.update_all_global_descriptors(
		rtg,
		pipeline_name_to_index["A3LambertianPipeline"],
		lambertian_pipeline.block_descriptor_set_name_to_index["Global"],
		lambertian_pipeline.block_binding_name_to_index["ShadowSphereLights"],
		"ShadowSphereLights"
	);
	workspace_manager.update_all_global_descriptors(
		rtg,
		pipeline_name_to_index["A3LambertianPipeline"],
		lambertian_pipeline.block_descriptor_set_name_to_index["Global"],
		lambertian_pipeline.block_binding_name_to_index["ShadowSpotLights"],
		"ShadowSpotLights"
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A3PBRPipeline"], 
		pbr_pipeline.block_descriptor_set_name_to_index["Global"], 
		pbr_pipeline.block_binding_name_to_index["PV"], 
		"PV"
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A3PBRPipeline"], 
		pbr_pipeline.block_descriptor_set_name_to_index["Global"], 
		pbr_pipeline.block_binding_name_to_index["SunLights"], 
		"SunLights"
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A3PBRPipeline"], 
		pbr_pipeline.block_descriptor_set_name_to_index["Global"], 
		pbr_pipeline.block_binding_name_to_index["SphereLights"], 
		"SphereLights"
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A3PBRPipeline"], 
		pbr_pipeline.block_descriptor_set_name_to_index["Global"], 
		pbr_pipeline.block_binding_name_to_index["SpotLights"], 
		"SpotLights"
	);
	workspace_manager.update_all_global_descriptors(
		rtg,
		pipeline_name_to_index["A3PBRPipeline"],
		pbr_pipeline.block_descriptor_set_name_to_index["Global"],
		pbr_pipeline.block_binding_name_to_index["ShadowSunLights"],
		"ShadowSunLights"
	);
	workspace_manager.update_all_global_descriptors(
		rtg,
		pipeline_name_to_index["A3PBRPipeline"],
		pbr_pipeline.block_descriptor_set_name_to_index["Global"],
		pbr_pipeline.block_binding_name_to_index["ShadowSphereLights"],
		"ShadowSphereLights"
	);
	workspace_manager.update_all_global_descriptors(
		rtg,
		pipeline_name_to_index["A3PBRPipeline"],
		pbr_pipeline.block_descriptor_set_name_to_index["Global"],
		pbr_pipeline.block_binding_name_to_index["ShadowSpotLights"],
		"ShadowSpotLights"
	);
	workspace_manager.update_all_global_descriptors(
		rtg,
		pipeline_name_to_index["A3SpotShadowPipeline"],
		spot_shadow_pipeline.block_descriptor_set_name_to_index["Global"],
		spot_shadow_pipeline.block_binding_name_to_index["ShadowSpotLights"],
		"ShadowSpotLights"
	);

	scene_manager.create(rtg, doc);
}

A3::~A3() {
	//just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) {
		std::cerr << "Failed to vkDeviceWaitIdle in A3::~A3 [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	texture_manager.destroy(rtg);

	scene_manager.destroy(rtg);

	framebuffer_manager.destroy(rtg);
	shadow_map_manager.destroy(rtg);

	background_pipeline.destroy(rtg);

	lambertian_pipeline.destroy(rtg);

	pbr_pipeline.destroy(rtg);

	spot_shadow_pipeline.destroy(rtg);

	tonemapping_pipeline.destroy(rtg);

	workspace_manager.destroy(rtg);

	render_pass_manager.destroy(rtg);

	query_pool_manager.destroy(rtg);
}

void A3::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain) {
	render_pass_manager.update_scissor_and_viewport(rtg_, swapchain.extent, camera_manager.get_aspect_ratio(rtg.configuration.open_debug_camera, swapchain.extent));
	framebuffer_manager.create(rtg_, swapchain, render_pass_manager);

	{
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


void A3::render(RTG &rtg_, RTG::RenderParams const &render_params) {
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
			assert(workspace.global_buffer_pairs["PV"]->host.size == sizeof(A3CommonData::PV));
			workspace.write_global_buffer(rtg, "PV", &pv_matrix, sizeof(A3CommonData::PV));
			assert(workspace.global_buffer_pairs["PV"]->host.size == workspace.global_buffer_pairs["PV"]->device.size);

			assert(workspace.global_buffer_pairs["SunLights"]->host.size >= sun_lights_bytes.size());
			workspace.write_global_buffer(rtg, "SunLights", sun_lights_bytes.data(), sun_lights_bytes.size());
			assert(workspace.global_buffer_pairs["SunLights"]->host.size == workspace.global_buffer_pairs["SunLights"]->device.size);

			assert(workspace.global_buffer_pairs["SphereLights"]->host.size >= sphere_lights_bytes.size());
			workspace.write_global_buffer(rtg, "SphereLights", sphere_lights_bytes.data(), sphere_lights_bytes.size());
			assert(workspace.global_buffer_pairs["SphereLights"]->host.size == workspace.global_buffer_pairs["SphereLights"]->device.size);

			assert(workspace.global_buffer_pairs["SpotLights"]->host.size >= spot_lights_bytes.size());
			workspace.write_global_buffer(rtg, "SpotLights", spot_lights_bytes.data(), spot_lights_bytes.size());
			assert(workspace.global_buffer_pairs["SpotLights"]->host.size == workspace.global_buffer_pairs["SpotLights"]->device.size);

			assert(workspace.global_buffer_pairs["ShadowSunLights"]->host.size >= shadow_sun_lights_bytes.size());
			workspace.write_global_buffer(rtg, "ShadowSunLights", shadow_sun_lights_bytes.data(), shadow_sun_lights_bytes.size());
			assert(workspace.global_buffer_pairs["ShadowSunLights"]->host.size == workspace.global_buffer_pairs["ShadowSunLights"]->device.size);

			assert(workspace.global_buffer_pairs["ShadowSphereLights"]->host.size >= shadow_sphere_lights_bytes.size());
			workspace.write_global_buffer(rtg, "ShadowSphereLights", shadow_sphere_lights_bytes.data(), shadow_sphere_lights_bytes.size());
			assert(workspace.global_buffer_pairs["ShadowSphereLights"]->host.size == workspace.global_buffer_pairs["ShadowSphereLights"]->device.size);

			assert(workspace.global_buffer_pairs["ShadowSpotLights"]->host.size >= shadow_spot_lights_bytes.size());
			workspace.write_global_buffer(rtg, "ShadowSpotLights", shadow_spot_lights_bytes.data(), shadow_spot_lights_bytes.size());
			assert(workspace.global_buffer_pairs["ShadowSpotLights"]->host.size == workspace.global_buffer_pairs["ShadowSpotLights"]->device.size);
		}

		{ //upload transforms for all pipelines
			auto upload_transforms = [&](const char* pipeline_name, auto& instances, const auto& pipeline) {
				if (instances.empty()) return;
				
				size_t needed_bytes = instances.size() * sizeof(A3CommonData::Transform);
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

				std::vector<A3CommonData::Transform> transform_data;
				transform_data.reserve(instances.size());
				for (const auto& inst : instances) {
					transform_data.push_back(inst.object_transform);
				}
				
				workspace.write_buffer(rtg, pipeline_idx, set_idx, binding_idx, transform_data.data(), needed_bytes);
			};

			upload_transforms("A3LambertianPipeline", lambertian_object_instances, lambertian_pipeline);
			upload_transforms("A3PBRPipeline", pbr_object_instances, pbr_pipeline);
			upload_transforms("A3SpotShadowPipeline", shadow_object_instances, spot_shadow_pipeline);
		}

		{ //memory barrier to make sure copies complete before rendering happens:
			VkMemoryBarrier memory_barrier{
				.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
			};

			vkCmdPipelineBarrier( workspace.command_buffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, //srcStageMask
				VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, //dstStageMask
				0, //dependencyFlags
				1, &memory_barrier, //memoryBarriers (count, data)
				0, nullptr, //bufferMemoryBarriers (count, data)
				0, nullptr //imageMemoryBarriers (count, data)
			);
		}

		// =====================================================================
		// Spot shadow pass: render depth for each shadow-casting spot light
		// =====================================================================
		{
			VkClearValue shadow_clear_value{
				.depthStencil{ .depth = 1.0f, .stencil = 0 },
			};

			const uint32_t shadow_count = std::min(
				static_cast<uint32_t>(shadow_map_manager.spot_shadow_targets.size()),
				static_cast<uint32_t>(shadow_spot_lights.size())
			);

			for (uint32_t light_index = 0; light_index < shadow_count; ++light_index) {
				auto const &shadow_target = shadow_map_manager.spot_shadow_targets[light_index];
				if (shadow_target.framebuffer == VK_NULL_HANDLE || shadow_target.resolution == 0) {
					continue;
				}

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
					VkRect2D shadow_scissor{
						.offset = {.x = 0, .y = 0},
						.extent = shadow_extent,
					};
					VkViewport shadow_viewport{
						.x = 0.0f,
						.y = 0.0f,
						.width = static_cast<float>(shadow_target.resolution),
						.height = static_cast<float>(shadow_target.resolution),
						.minDepth = 0.0f,
						.maxDepth = 1.0f,
					};

					vkCmdSetScissor(workspace.command_buffer, 0, 1, &shadow_scissor);
					vkCmdSetViewport(workspace.command_buffer, 0, 1, &shadow_viewport);

					if (!shadow_object_instances.empty()) {
						vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spot_shadow_pipeline.pipeline);

						std::array< VkBuffer, 1 > vertex_buffers{ scene_manager.vertex_buffer.handle };
						std::array< VkDeviceSize, 1 > offsets{ 0 };
						vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());

						auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A3SpotShadowPipeline"]][spot_shadow_pipeline.block_descriptor_set_name_to_index["Global"]].descriptor_set;
						auto &transform_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A3SpotShadowPipeline"]][spot_shadow_pipeline.block_descriptor_set_name_to_index["Transforms"]].descriptor_set;

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

						A3SpotShadowPipeline::Push push{
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
								workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A3BackgroundPipeline"]][background_pipeline.block_descriptor_set_name_to_index["PV"]].descriptor_set, //0: PV
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

				{ // draw with the lambertian pipeline:
					if (!lambertian_object_instances.empty()) { //draw with the objects pipeline:
						vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lambertian_pipeline.pipeline);

						{ //use object_vertices (offset 0) as vertex buffer binding 0:
							std::array< VkBuffer, 1 > vertex_buffers{ scene_manager.vertex_buffer.handle };
							std::array< VkDeviceSize, 1 > offsets{ 0 };
							vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
						}

						{ //bind Global and Transforms descriptor_set sets:
							auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A3LambertianPipeline"]][lambertian_pipeline.block_descriptor_set_name_to_index["Global"]].descriptor_set;
							auto &transform_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A3LambertianPipeline"]][lambertian_pipeline.block_descriptor_set_name_to_index["Transforms"]].descriptor_set;
							auto &textures_descriptor_set = lambertian_pipeline.set2_Textures_instance;

							std::array< VkDescriptorSet, 3 > descriptor_sets{
								global_descriptor_set, //0: Global (PV, Light)
								transform_descriptor_set, //1: Transforms
								textures_descriptor_set, //2: Textures
							};
							vkCmdBindDescriptorSets(
								workspace.command_buffer, //command buffer
								VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
								lambertian_pipeline.layout, //pipeline layout
								0, //first set
								uint32_t(descriptor_sets.size()), descriptor_sets.data(), //descriptor_set sets count, ptr
								0, nullptr //dynamic offsets count, ptr
							);
						}

						for(uint32_t i = 0; i < lambertian_object_instances.size(); ++i) {
							//draw all instances:
							A3LambertianPipeline::Push push{
								.MATERIAL_INDEX = static_cast<uint32_t>(lambertian_object_instances[i].material_index * 5)
							};
							vkCmdPushConstants(workspace.command_buffer, lambertian_pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
							vkCmdDraw(workspace.command_buffer, lambertian_object_instances[i].object_ranges.count, 1, lambertian_object_instances[i].object_ranges.first, i);
						}
					}
				}

				{ // draw with the PBR pipeline:
					if (!pbr_object_instances.empty()) { //draw with the objects pipeline:
						vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_pipeline.pipeline);

						{ //use object_vertices (offset 0) as vertex buffer binding 0:
							std::array< VkBuffer, 1 > vertex_buffers{ scene_manager.vertex_buffer.handle };
							std::array< VkDeviceSize, 1 > offsets{ 0 };
							vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
						}

						{ //bind Global and Transforms descriptor_set sets:
							auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A3PBRPipeline"]][pbr_pipeline.block_descriptor_set_name_to_index["Global"]].descriptor_set;
							auto &transform_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A3PBRPipeline"]][pbr_pipeline.block_descriptor_set_name_to_index["Transforms"]].descriptor_set;
							auto &textures_descriptor_set = pbr_pipeline.set2_Textures_instance;

							std::array< VkDescriptorSet, 3 > descriptor_sets{
								global_descriptor_set, //0: Global (PV, Light)
								transform_descriptor_set, //1: Transforms
								textures_descriptor_set, //2: Textures
							};
							vkCmdBindDescriptorSets(
								workspace.command_buffer, //command buffer
								VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
								pbr_pipeline.layout, //pipeline layout
								0, //first set
								uint32_t(descriptor_sets.size()), descriptor_sets.data(), //descriptor_set sets count, ptr
								0, nullptr //dynamic offsets count, ptr
							);
						}

						for(uint32_t i = 0; i < pbr_object_instances.size(); ++i) {
							//draw all instances:
							A3PBRPipeline::Push push{
								.MATERIAL_INDEX = static_cast<uint32_t>(1 + pbr_object_instances[i].material_index * 5)
							};
							vkCmdPushConstants(workspace.command_buffer, pbr_pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
							vkCmdDraw(workspace.command_buffer, pbr_object_instances[i].object_ranges.count, 1, pbr_object_instances[i].object_ranges.first, i);
						}
					}
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

				A3ToneMappingPipeline::Push push{
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


void A3::update(float dt) {
	time = std::fmod(time + dt, 8.0f);

	SceneTree::update_animation(doc, time);
	SceneTree::traverse_scene(doc, mesh_tree_data, light_tree_data, camera_tree_data, environment_tree_data);

	// Update camera
	camera_manager.update(dt, camera_tree_data, rtg.configuration.open_debug_camera);

	{ // update global data
		pv_matrix.PERSPECTIVE = rtg.configuration.open_debug_camera ? camera_manager.get_debug_perspective() : camera_manager.get_perspective();
		pv_matrix.VIEW = rtg.configuration.open_debug_camera ? camera_manager.get_debug_view() : camera_manager.get_view();
		pv_matrix.CAMERA_POSITION = rtg.configuration.open_debug_camera ? glm::vec4(camera_manager.get_debug_camera().camera_position, 1.0f) : glm::vec4(camera_manager.get_active_camera().camera_position, 1.0f);

		size_t sun_idx = 0;
		size_t sphere_idx = 0;
		size_t spot_idx = 0;
		size_t shadow_sun_idx = 0;
		size_t shadow_sphere_idx = 0;
		size_t shadow_spot_idx = 0;

		for (const auto &ltd : light_tree_data) {
			if (ltd.light_index >= doc->lights.size()) continue;

			const auto &src_light = doc->lights[ltd.light_index];
			const bool has_shadow = (src_light.shadow != 0);
			const glm::mat4 transform = ltd.model_matrix;
			const glm::mat3 blender_rotation = glm::mat3(transform);
			const glm::vec3 blender_forward = blender_rotation * glm::vec3{0.0f, 0.0f, 1.0f};
			const glm::vec3 position = BLENDER_TO_VULKAN_3 * glm::vec3{transform[3][0], transform[3][1], transform[3][2]};
			const glm::vec3 direction = glm::normalize(BLENDER_TO_VULKAN_3 * blender_forward);
			const glm::vec3 up = BLENDER_TO_VULKAN_3 * blender_rotation * glm::vec3{0.0f, 1.0f, 0.0f};

			if (src_light.sun) {
				auto &dst = has_shadow ? shadow_sun_lights.at(shadow_sun_idx++) : sun_lights.at(sun_idx++);
				dst.direction = direction;
			}

			if (src_light.sphere) {
				auto &dst = has_shadow ? shadow_sphere_lights.at(shadow_sphere_idx++) : sphere_lights.at(sphere_idx++);
				dst.position = position;
			}

			if (src_light.spot) {
				auto &dst = has_shadow ? shadow_spot_lights.at(shadow_spot_idx++) : spot_lights.at(spot_idx++);
				dst.position = position;
				dst.direction = direction;
				const float near_plane = 0.1f;
				const float far_plane = 1000.0f;
				const glm::vec3 view_direct = glm::normalize(BLENDER_TO_VULKAN_3 * blender_rotation * glm::vec3{0.0f, 0.0f, -1.0f});
				glm::mat4 view = glm::lookAtRH(position, position + view_direct, up);
				glm::mat4 proj = glm::perspectiveRH_ZO(dst.fov, 1.0f, near_plane, far_plane);
				proj[1][1] *= -1.0f;
				dst.perspective = proj * view;
			}
		}

		auto overwrite_lights_payload = [](auto const &lights, std::vector<uint8_t> &bytes) {
			using LightT = typename std::decay_t<decltype(lights)>::value_type;
			const size_t header_size = sizeof(A3CommonData::LightsHeader);
			const size_t payload_size = sizeof(LightT) * lights.size();
			assert(bytes.size() == header_size + payload_size);
			if (!lights.empty()) {
				std::memcpy(bytes.data() + header_size, lights.data(), payload_size);
			}
		};

		overwrite_lights_payload(sun_lights, sun_lights_bytes);
		overwrite_lights_payload(sphere_lights, sphere_lights_bytes);
		overwrite_lights_payload(spot_lights, spot_lights_bytes);
		overwrite_lights_payload(shadow_sun_lights, shadow_sun_lights_bytes);
		overwrite_lights_payload(shadow_sphere_lights, shadow_sphere_lights_bytes);
		overwrite_lights_payload(shadow_spot_lights, shadow_spot_lights_bytes);
	}

	{ // update object instances with frustum culling
		pbr_object_instances.clear();
		lambertian_object_instances.clear();
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
			}
		}
	}
}


void A3::on_input(InputEvent const &event) {
	camera_manager.on_input(event);

	if (event.type == InputEvent::KeyDown) {
	// Change active camera with TAB
		if (event.key.key == GLFW_KEY_TAB) {
			camera_manager.change_active_camera();
			render_pass_manager.update_scissor_and_viewport(rtg, rtg.swapchain_extent, camera_manager.get_aspect_ratio(rtg.configuration.open_debug_camera, rtg.swapchain_extent));
		}

		if (event.key.key == GLFW_KEY_LEFT_ALT){
			rtg.configuration.open_debug_camera = !rtg.configuration.open_debug_camera;
			render_pass_manager.update_scissor_and_viewport(rtg, rtg.swapchain_extent, camera_manager.get_aspect_ratio(rtg.configuration.open_debug_camera, rtg.swapchain_extent));
		}
	}
}
