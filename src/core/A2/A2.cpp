#ifdef _WIN32
//ensure we have M_PI
#define _USE_MATH_DEFINES
#endif

#include "A2.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

A2::A2(RTG &rtg) : A2(rtg, "origin-check.s72") {
}

A2::A2(RTG &rtg, const std::string &filename) : 
	rtg{rtg}, 
	doc{S72Loader::load_file(s72_dir + filename)}, 
	camera_manager{}, 
	workspace_manager{}, 
	render_pass_manager{},
	background_pipeline{},
	pbr_pipeline{},
	reflection_pipeline{},
	scene_manager{},
	texture_manager{},
	framebuffer_manager{}
{
	render_pass_manager.create(rtg);

	texture_manager.create(rtg, doc);

	background_pipeline.create(rtg, render_pass_manager.render_pass, 0, texture_manager);

	pbr_pipeline.create(rtg, render_pass_manager.render_pass, 0, texture_manager);

	reflection_pipeline.create(rtg, render_pass_manager.render_pass, 0, texture_manager);

	std::vector< std::vector< Pipeline::BlockDescriptorConfig > > block_descriptor_configs_by_pipeline{3};
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["A2BackgroundPipeline"]] = background_pipeline.block_descriptor_configs;
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["A2PBRPipeline"]] = pbr_pipeline.block_descriptor_configs;
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["A2ReflectionPipeline"]] = reflection_pipeline.block_descriptor_configs;

	std::vector< WorkspaceManager::GlobalBufferConfig > global_buffer_configs{
		WorkspaceManager::GlobalBufferConfig{
			.name = "PV",
			.size = sizeof(CommonData::PV),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		},
		WorkspaceManager::GlobalBufferConfig{
			.name = "Light",
			.size = sizeof(CommonData::Light),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		},
	};

	workspace_manager.create(rtg, std::move(block_descriptor_configs_by_pipeline), std::move(global_buffer_configs), 2);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A2BackgroundPipeline"], 
		background_pipeline.block_descriptor_set_name_to_index["PV"], 
		background_pipeline.block_binding_name_to_index["PV"], 
		"PV",
		sizeof(CommonData::PV)
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A2PBRPipeline"], 
		pbr_pipeline.block_descriptor_set_name_to_index["Global"], 
		pbr_pipeline.block_binding_name_to_index["PV"], 
		"PV",
		sizeof(CommonData::PV)
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A2PBRPipeline"], 
		pbr_pipeline.block_descriptor_set_name_to_index["Global"], 
		pbr_pipeline.block_binding_name_to_index["Light"], 
		"Light",
		sizeof(CommonData::Light)
	);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A2ReflectionPipeline"], 
		reflection_pipeline.block_descriptor_set_name_to_index["PV"], 
		reflection_pipeline.block_binding_name_to_index["PV"], 
		"PV",
		sizeof(CommonData::PV)
	);

	scene_manager.create(rtg, doc);

	camera_manager.create(doc, rtg.swapchain_extent.width, rtg.swapchain_extent.height);
}

A2::~A2() {
	//just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) {
		std::cerr << "Failed to vkDeviceWaitIdle in A2::~A2 [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	texture_manager.destroy(rtg);

	scene_manager.destroy(rtg);

	framebuffer_manager.destroy(rtg);

	background_pipeline.destroy(rtg);

	pbr_pipeline.destroy(rtg);

	reflection_pipeline.destroy(rtg);

	workspace_manager.destroy(rtg);

	render_pass_manager.destroy(rtg);
}

void A2::on_swapchain(RTG &rtg, RTG::SwapchainEvent const &swapchain) {
	framebuffer_manager.create(rtg, swapchain, render_pass_manager);
	camera_manager.resize_all_cameras(swapchain.extent.width, swapchain.extent.height);
	render_pass_manager.update_scissor_and_viewport(rtg, swapchain.extent);
}


void A2::render(RTG &rtg_, RTG::RenderParams const &render_params) {
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

		{ //upload global data:
			assert(workspace.global_buffer_pairs["PV"]->host.size == sizeof(CommonData::PV));
			workspace.write_global_buffer(rtg, "PV", &pv_matrix, sizeof(CommonData::PV));
			assert(workspace.global_buffer_pairs["PV"]->host.size == workspace.global_buffer_pairs["PV"]->device.size);

			assert(workspace.global_buffer_pairs["Light"]->host.size == sizeof(CommonData::Light));
			workspace.write_global_buffer(rtg, "Light", &light, sizeof(CommonData::Light));
			assert(workspace.global_buffer_pairs["Light"]->host.size == workspace.global_buffer_pairs["Light"]->device.size);
		}

		{ //upload reflection transforms
			if (!reflection_object_instances.empty()) { 
				size_t needed_bytes = reflection_object_instances.size() * sizeof(CommonData::Transform);

				auto& buffer_pair = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A2ReflectionPipeline"]][reflection_pipeline.block_descriptor_set_name_to_index["Transforms"]].buffer_pairs[reflection_pipeline.block_binding_name_to_index["Transforms"]];
				if (buffer_pair->host.handle == VK_NULL_HANDLE || buffer_pair->host.size < needed_bytes) {
					//round to next multiple of 4k to avoid re-allocating continuously if vertex count grows slowly:
					size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
					workspace.update_descriptor(
						rtg, 
						pipeline_name_to_index["A2ReflectionPipeline"], 
						reflection_pipeline.block_descriptor_set_name_to_index["Transforms"], 
						reflection_pipeline.block_binding_name_to_index["Transforms"],
						new_bytes
					);
				}

				{
					assert(buffer_pair->host.size == buffer_pair->device.size);
					assert(buffer_pair->host.size >= needed_bytes);
					assert(buffer_pair->host.allocation.mapped);

					std::vector<CommonData::Transform> transform_data;

					for (const auto& inst : reflection_object_instances) {
						transform_data.push_back(inst.object_transform);
					}
					
					workspace.write_buffer(rtg, pipeline_name_to_index["A2ReflectionPipeline"], 
						reflection_pipeline.block_descriptor_set_name_to_index["Transforms"], 
						reflection_pipeline.block_binding_name_to_index["Transforms"],
						transform_data.data(),
						needed_bytes
					);
				}	
			}
		}

		{ //upload pbr transforms
			if (!pbr_object_instances.empty()) { 
				size_t needed_bytes = pbr_object_instances.size() * sizeof(CommonData::Transform);
				
				auto& buffer_pair = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A2PBRPipeline"]][pbr_pipeline.block_descriptor_set_name_to_index["Transforms"]].buffer_pairs[pbr_pipeline.block_binding_name_to_index["Transforms"]];
				if (buffer_pair->host.handle == VK_NULL_HANDLE || buffer_pair->host.size < needed_bytes) {
					//round to next multiple of 4k to avoid re-allocating continuously if vertex count grows slowly:
					size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
					workspace.update_descriptor(
						rtg, 
						pipeline_name_to_index["A2PBRPipeline"], 
						pbr_pipeline.block_descriptor_set_name_to_index["Transforms"], 
						pbr_pipeline.block_binding_name_to_index["Transforms"],
						new_bytes
					);
				}

				{
					assert(buffer_pair->host.size == buffer_pair->device.size);
					assert(buffer_pair->host.size >= needed_bytes);
					assert(buffer_pair->host.allocation.mapped);

					std::vector<CommonData::Transform> transform_data;

					for (const auto& inst : pbr_object_instances) {
						transform_data.push_back(inst.object_transform);
					}
					
					workspace.write_buffer(rtg, pipeline_name_to_index["A2PBRPipeline"], 
						pbr_pipeline.block_descriptor_set_name_to_index["Transforms"], 
						pbr_pipeline.block_binding_name_to_index["Transforms"],
						transform_data.data(),
						needed_bytes
					);
				}	
			}
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

		
		{ //render pass
			VkRenderPassBeginInfo begin_info{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = render_pass_manager.render_pass,
				.framebuffer = framebuffer,
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
					vkCmdSetScissor(workspace.command_buffer, 0, 1, &render_pass_manager.scissor);
				}
				{ //configure viewport transform:
					vkCmdSetViewport(workspace.command_buffer, 0, 1, &render_pass_manager.viewport);
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
								workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A2BackgroundPipeline"]][background_pipeline.block_descriptor_set_name_to_index["PV"]].descriptor_set, //0: PV
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

				{ //draw with the reflection pipeline:
					if (!reflection_object_instances.empty()) { //draw with the objects pipeline:
						vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, reflection_pipeline.pipeline);

						{ //use object_vertices (offset 0) as vertex buffer binding 0:
							std::array< VkBuffer, 1 > vertex_buffers{ scene_manager.vertex_buffer.handle };
							std::array< VkDeviceSize, 1 > offsets{ 0 };
							vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
						}

						{ //bind Transforms descriptor_set set:
							auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A2ReflectionPipeline"]][reflection_pipeline.block_descriptor_set_name_to_index["Global"]].descriptor_set;
							auto &transform_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A2ReflectionPipeline"]][reflection_pipeline.block_descriptor_set_name_to_index["Transforms"]].descriptor_set;
							auto &textures_descriptor_set = reflection_pipeline.set2_CUBEMAP_instance;
							std::array< VkDescriptorSet, 3 > descriptor_sets{
								global_descriptor_set, //0: Global (PV)
								transform_descriptor_set, //1: Transforms
								textures_descriptor_set, //2: CUBEMAP
							};
							vkCmdBindDescriptorSets(
								workspace.command_buffer, //command buffer
								VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
								reflection_pipeline.layout, //pipeline layout
								0, //first set
								uint32_t(descriptor_sets.size()), descriptor_sets.data(), //descriptor_set sets count, ptr
								0, nullptr //dynamic offsets count, ptr
							);
						}

						//draw all instances:
						for (size_t i = 0; i < reflection_object_instances.size(); ++i) {
							vkCmdDraw(workspace.command_buffer, reflection_object_instances[i].object_ranges.count, 1, reflection_object_instances[i].object_ranges.first, i);
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
							auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A2PBRPipeline"]][pbr_pipeline.block_descriptor_set_name_to_index["Global"]].descriptor_set;
							auto &transform_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A2PBRPipeline"]][pbr_pipeline.block_descriptor_set_name_to_index["Transforms"]].descriptor_set;
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

						for(size_t i = 0; i < pbr_object_instances.size(); ++i) {
							//draw all instances:
							A2PBRPipeline::Push push{
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
}


void A2::update(float dt) {
	time = std::fmod(time + dt, 60.0f);

	// Update camera
	camera_manager.update(dt);

	{ // update global data
		pv_matrix.PERSPECTIVE = camera_manager.get_perspective();
		pv_matrix.VIEW = camera_manager.get_view();
		pv_matrix.CAMERA_POSITION = glm::vec4(camera_manager.get_active_camera().camera_position, 1.0f);

		light = CommonData::Light{
			.LIGHT_POSITION = doc->lights[0].transforms[0][3],
			.LIGHT_ENERGY = glm::vec4(doc->lights[0].tint, 1.0f),
		}; // assuming single light for now
	}

	{ // update object instances with frustum culling
		reflection_object_instances.clear();
		pbr_object_instances.clear();
		
		// Get frustum for culling
		auto frustum = camera_manager.get_frustum();

		for(uint32_t i = 0; i < doc->meshes.size(); ++i) 
		{
			const auto& mesh = doc->meshes[i];
			const auto& object_range = scene_manager.object_ranges[i];
			std::optional<S72Loader::Material> material = mesh.material_index.has_value() ? std::optional<S72Loader::Material>{doc->materials[mesh.material_index.value()]} : std::nullopt;

			for(auto &transform : mesh.transforms){
				glm::mat4 MODEL = BLENDER_TO_VULKAN_4 * transform;

				// Transform local AABB to world AABB (8 corners method)
				const glm::vec3& bmin = object_range.aabb_min;
				const glm::vec3& bmax = object_range.aabb_max;
				glm::vec3 corners[8] = {
					{bmin.x, bmin.y, bmin.z}, {bmax.x, bmin.y, bmin.z}, {bmin.x, bmax.y, bmin.z}, {bmax.x, bmax.y, bmin.z},
					{bmin.x, bmin.y, bmax.z}, {bmax.x, bmin.y, bmax.z}, {bmin.x, bmax.y, bmax.z}, {bmax.x, bmax.y, bmax.z}
				};
				glm::vec3 world_min(FLT_MAX);
				glm::vec3 world_max(-FLT_MAX);
				for (int c = 0; c < 8; ++c) {
					glm::vec3 wp = glm::vec3(MODEL * glm::vec4(corners[c], 1.0f));
					world_min = glm::min(world_min, wp);
					world_max = glm::max(world_max, wp);
				}

				// Frustum culling check with world-space AABB
				if (!frustum.is_box_visible(world_min, world_max)) {
					continue;
				}

				glm::mat4 MODEL_NORMAL = glm::transpose(glm::inverse(MODEL));

				// reflective or environment material instance
				if(material.has_value() && (material->mirror || material->environment)) {
					ReflectionInstance reflection_inst{
						.object_ranges = object_range,
						.object_transform{
							.MODEL = MODEL,
							.MODEL_NORMAL = MODEL_NORMAL,
						},
						.material_index = mesh.material_index.value_or(0),
					};

					if (material->mirror) {
						reflection_inst.object_transform.MODEL_NORMAL[3][3] = 1.0f; // reflective
					} else {
						reflection_inst.object_transform.MODEL_NORMAL[3][3] = 0.0f; // non-reflective
					}

					reflection_object_instances.emplace_back(std::move(reflection_inst));
				}

				if(material.has_value() && material->pbr) {
					PBRInstance pbr_inst{
						.object_ranges = object_range,
						.object_transform{
							.MODEL = MODEL,
							.MODEL_NORMAL = MODEL_NORMAL,
						},
						.material_index = mesh.material_index.value_or(0),
					};

					pbr_object_instances.emplace_back(std::move(pbr_inst));
				}
			}
		}
	}
}


void A2::on_input(InputEvent const &event) {
	camera_manager.on_input(event);
}
