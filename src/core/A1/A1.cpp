#ifdef _WIN32
//ensure we have M_PI
#define _USE_MATH_DEFINES
#endif

#include "A1.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

A1::A1(RTG &rtg) : A1(rtg, "origin-check.s72") {
}

A1::A1(RTG &rtg, const std::string &filename) : 
	rtg{rtg}, 
	doc{S72Loader::load_file(s72_dir + filename)}, 
	camera_manager{}, 
	workspace_manager{}, 
	render_pass_manager{}, 
	objects_pipeline{},
	scene_manager{},
	texture_manager{},
	framebuffer_manager{},
	mesh_tree_data{},
	light_tree_data{},
	camera_tree_data{},
	environment_tree_data{}
{
	SceneTree::traverse_scene(doc, mesh_tree_data, light_tree_data, camera_tree_data, environment_tree_data);

	render_pass_manager.create(rtg);

	texture_manager.create(rtg, doc, 1);

	objects_pipeline.create(rtg, render_pass_manager.render_pass, 0, texture_manager);

	std::vector< std::vector< Pipeline::BlockDescriptorConfig > > block_descriptor_configs_by_pipeline;
	block_descriptor_configs_by_pipeline.push_back(objects_pipeline.block_descriptor_configs);

	std::vector< WorkspaceManager::GlobalBufferConfig > global_buffer_configs{
		WorkspaceManager::GlobalBufferConfig{
			.name = "PV",
			.size = sizeof(A1ObjectsPipeline::PV),
			.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
		}
	};

	workspace_manager.create(rtg, std::move(block_descriptor_configs_by_pipeline), std::move(global_buffer_configs), 2);
	workspace_manager.update_all_global_descriptors(
		rtg, 
		pipeline_name_to_index["A1ObjectsPipeline"], 
		objects_pipeline.block_descriptor_set_name_to_index["PV"], 
		objects_pipeline.block_binding_name_to_index["PV"], 
		"PV",
		sizeof(A1ObjectsPipeline::PV)
	);

	scene_manager.create(rtg, doc);

	camera_manager.create(doc, rtg.swapchain_extent.width, rtg.swapchain_extent.height, this->camera_tree_data, rtg.configuration.init_camera_name);
}

A1::~A1() {
	//just in case rendering is still in flight, don't destroy resources:
	//(not using VK macro to avoid throw-ing in destructor)
	if (VkResult result = vkDeviceWaitIdle(rtg.device); result != VK_SUCCESS) {
		std::cerr << "Failed to vkDeviceWaitIdle in A1::~A1 [" << string_VkResult(result) << "]; continuing anyway." << std::endl;
	}

	texture_manager.destroy(rtg);

	scene_manager.destroy(rtg);

	framebuffer_manager.destroy(rtg);

	objects_pipeline.destroy(rtg);

	workspace_manager.destroy(rtg);

	render_pass_manager.destroy(rtg);
}

void A1::on_swapchain(RTG &rtg_, RTG::SwapchainEvent const &swapchain) {
	framebuffer_manager.create(rtg_, swapchain, render_pass_manager);
	camera_manager.resize_all_cameras(swapchain.extent.width, swapchain.extent.height);
	render_pass_manager.update_scissor_and_viewport(rtg_, swapchain.extent);
}


void A1::render(RTG &rtg_, RTG::RenderParams const &render_params) {
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

		{ //upload world info:
			assert(workspace.global_buffer_pairs["PV"]->host.size == sizeof(A1ObjectsPipeline::PV));
			workspace.write_global_buffer(rtg, "PV", &pv_matrix, sizeof(A1ObjectsPipeline::PV));
			assert(workspace.global_buffer_pairs["PV"]->host.size == workspace.global_buffer_pairs["PV"]->device.size);
		}

		{ //upload object transforms
			if (!object_instances.empty()) { 
				size_t needed_bytes = object_instances.size() * sizeof(A1ObjectsPipeline::Transform);

				auto& buffer_pair = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A1ObjectsPipeline"]][objects_pipeline.block_descriptor_set_name_to_index["Transforms"]].buffer_pairs[objects_pipeline.block_binding_name_to_index["Transforms"]];
				if (buffer_pair->host.handle == VK_NULL_HANDLE || buffer_pair->host.size < needed_bytes) {
					//round to next multiple of 4k to avoid re-allocating continuously if vertex count grows slowly:
					size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
					workspace.update_descriptor(
						rtg, 
						pipeline_name_to_index["A1ObjectsPipeline"], 
						objects_pipeline.block_descriptor_set_name_to_index["Transforms"], 
						objects_pipeline.block_binding_name_to_index["Transforms"],
						new_bytes
					);
				}

				{
					assert(buffer_pair->host.size == buffer_pair->device.size);
					assert(buffer_pair->host.size >= needed_bytes);
					assert(buffer_pair->host.allocation.mapped);

					std::vector<A1ObjectsPipeline::Transform> transform_data;

					for (const auto& inst : object_instances) {
						transform_data.push_back(inst.transform);
					}
					
					workspace.write_buffer(rtg, pipeline_name_to_index["A1ObjectsPipeline"], 
						objects_pipeline.block_descriptor_set_name_to_index["Transforms"], 
						objects_pipeline.block_binding_name_to_index["Transforms"],
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

				{ //draw with the objects pipeline:
					if (!object_instances.empty()) { //draw with the objects pipeline:
						vkCmdBindPipeline(workspace.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objects_pipeline.pipeline);

						{ //use object_vertices (offset 0) as vertex buffer binding 0:
							std::array< VkBuffer, 1 > vertex_buffers{ scene_manager.vertex_buffer.handle };
							std::array< VkDeviceSize, 1 > offsets{ 0 };
							vkCmdBindVertexBuffers(workspace.command_buffer, 0, uint32_t(vertex_buffers.size()), vertex_buffers.data(), offsets.data());
						}

						{ //bind Transforms descriptor set:
							auto &global_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A1ObjectsPipeline"]][objects_pipeline.block_descriptor_set_name_to_index["PV"]].descriptor_set;
							auto &transform_descriptor_set = workspace.pipeline_descriptor_set_groups[pipeline_name_to_index["A1ObjectsPipeline"]][objects_pipeline.block_descriptor_set_name_to_index["Transforms"]].descriptor_set;
							auto &textures_descriptor_set = objects_pipeline.set2_TEXTURE_instance;
							std::array< VkDescriptorSet, 3 > descriptor_sets{
								global_descriptor_set, //0: World
								transform_descriptor_set, //1: Transforms
								textures_descriptor_set //2: Textures
							};
							vkCmdBindDescriptorSets(
								workspace.command_buffer, //command buffer
								VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
								objects_pipeline.layout, //pipeline layout
								0, //first set
								uint32_t(descriptor_sets.size()), descriptor_sets.data(), //descriptor sets count, ptr
								0, nullptr //dynamic offsets count, ptr
							);
						}

						//draw all instances:
						for(uint32_t i = 0; i < object_instances.size(); ++i) {
							//draw all instances:
							A1ObjectsPipeline::Push push{
								.MATERIAL_INDEX = static_cast<uint32_t>(object_instances[i].material_index * 5 + 2) // albedo is at texture slot 2 + 5 * material_index
							};

							vkCmdPushConstants(workspace.command_buffer, objects_pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
							vkCmdDraw(workspace.command_buffer, object_instances[i].object_ranges.count, 1, object_instances[i].object_ranges.first, i);
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


void A1::update(float dt) {
	time = fmod(time + dt, 60.0f);

	SceneTree::traverse_scene(doc, mesh_tree_data, light_tree_data, camera_tree_data, environment_tree_data);
	SceneTree::update_animation(doc, time);

	// Update camera
	camera_manager.update(dt, camera_tree_data, rtg.configuration.open_debug_camera);

	{ // update global data
		pv_matrix.PERSPECTIVE = rtg.configuration.open_debug_camera ? camera_manager.get_debug_perspective() : camera_manager.get_perspective();
		pv_matrix.VIEW = rtg.configuration.open_debug_camera ? camera_manager.get_debug_view() : camera_manager.get_view();
	}

	{
		object_instances.clear();	

		for(auto mtd : mesh_tree_data){
			const size_t mesh_index = mtd.mesh_index;
			const size_t material_index = mtd.material_index;
			const glm::mat4 MODEL = BLENDER_TO_VULKAN_4 * mtd.model_matrix;
			const glm::mat4 MODEL_NORMAL = glm::transpose(glm::inverse(MODEL));
			const auto& object_range = doc->meshes[mesh_index].range;

				// Transform local AABB to world AABB (8 corners method)
				// const glm::vec3& bmin = object_range.aabb_min;
				// const glm::vec3& bmax = object_range.aabb_max;
				// glm::vec3 corners[8] = {
				// 	{bmin.x, bmin.y, bmin.z}, {bmax.x, bmin.y, bmin.z}, {bmin.x, bmax.y, bmin.z}, {bmax.x, bmax.y, bmin.z},
				// 	{bmin.x, bmin.y, bmax.z}, {bmax.x, bmin.y, bmax.z}, {bmin.x, bmax.y, bmax.z}, {bmax.x, bmax.y, bmax.z}
				// };
				// glm::vec3 world_min(FLT_MAX);
				// glm::vec3 world_max(-FLT_MAX);
				// for (int c = 0; c < 8; ++c) {
				// 	glm::vec3 wp = glm::vec3(MODEL * glm::vec4(corners[c], 1.0f));
				// 	world_min = glm::min(world_min, wp);
				// 	world_max = glm::max(world_max, wp);
				// }

				// Frustum culling check with world-space AABB
				// if (!frustum.is_box_visible(world_min, world_max)) {
				// 	continue;
				// }

			object_instances.emplace_back(ObjectInstance{
				.object_ranges = object_range,
				.transform{
					.MODEL = MODEL,
					.MODEL_NORMAL = MODEL_NORMAL,
				},
				.material_index = material_index,
			});
		}
	}
}


void A1::on_input(InputEvent const &event) {
	camera_manager.on_input(event);
}
