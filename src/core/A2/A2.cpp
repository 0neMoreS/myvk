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
	objects_pipeline{},
	scene_manager{},
	texture_manager{},
	framebuffer_manager{}
{
	render_pass_manager.create(rtg);

	background_pipeline.create(rtg, render_pass_manager.render_pass, 0);
	objects_pipeline.set3_CUBEMAP = background_pipeline.set1_CUBEMAP;
	objects_pipeline.create(rtg, render_pass_manager.render_pass, 0);

	std::vector< std::vector< Pipeline::BlockDescriptorConfig > > block_descriptor_configs_by_pipeline{2};
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["A2BackgroundPipeline"]] = background_pipeline.block_descriptor_configs;
	block_descriptor_configs_by_pipeline[pipeline_name_to_index["A2ObjectsPipeline"]] = objects_pipeline.block_descriptor_configs;

	std::vector< std::vector< Pipeline::TextureDescriptorConfig > > texture_descriptor_configs_by_pipeline{2};
	texture_descriptor_configs_by_pipeline[pipeline_name_to_index["A2ObjectsPipeline"]] = objects_pipeline.texture_descriptor_configs;

	workspace_manager.create(rtg, std::move(block_descriptor_configs_by_pipeline), 2);
	workspace_manager.update_all_descriptors(rtg, pipeline_name_to_index["A2BackgroundPipeline"], 0, sizeof(background_transform));
	workspace_manager.update_all_descriptors(rtg, pipeline_name_to_index["A2ObjectsPipeline"], 0, sizeof(object_world));

	scene_manager.create(rtg, doc);

	// Create texture manager to load all textures from document and create descriptor pool
	texture_manager.create(rtg, doc, std::move(texture_descriptor_configs_by_pipeline), background_pipeline.set1_CUBEMAP);

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

	objects_pipeline.destroy(rtg);

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
	workspace.reset_recoring();
	
	{ //begin recording:
		workspace.begin_recording();

		{ //upload background transform:
			assert(workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2BackgroundPipeline"]][0].host.size == sizeof(background_transform));

			//host-side copy into Transform_src:
			memcpy(workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2BackgroundPipeline"]][0].host.allocation.data(), &background_transform, sizeof(background_transform));

			//add device-side copy from Transform_src -> Transform:
			assert(workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2BackgroundPipeline"]][0].host.size == workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2BackgroundPipeline"]][0].device.size);

			workspace.copy_buffer(rtg, pipeline_name_to_index["A2BackgroundPipeline"], 0, sizeof(background_transform));
		}

		{ //upload world info:
			assert(workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][0].host.size == sizeof(object_world));

			//host-side copy into World_src:
			memcpy(workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][0].host.allocation.data(), &object_world, sizeof(object_world));

			//add device-side copy from World_src -> World:
			assert(workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][0].host.size == workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][0].device.size);

			workspace.copy_buffer(rtg, pipeline_name_to_index["A2ObjectsPipeline"], 0, sizeof(object_world));
		}

		{ //upload object transforms
			if (!object_instances.empty()) { 
				size_t needed_bytes = object_instances.size() * sizeof(A2ObjectsPipeline::Transform);
				if (workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][1].host.handle == VK_NULL_HANDLE || workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][1].host.size < needed_bytes) {
					//round to next multiple of 4k to avoid re-allocating continuously if vertex count grows slowly:
					size_t new_bytes = ((needed_bytes + 4096) / 4096) * 4096;
					workspace.update_descriptor(rtg, pipeline_name_to_index["A2ObjectsPipeline"], 1, new_bytes);
				}

				assert(workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][1].host.size == workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][1].device.size);
				assert(workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][1].host.size >= needed_bytes);
				{ //copy transforms into Transforms_src:
					assert(workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][1].host.allocation.mapped);
					A2ObjectsPipeline::Transform *out = reinterpret_cast< A2ObjectsPipeline::Transform * >(workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][1].host.allocation.data()); // Strict aliasing violation, but it doesn't matter
					for (ObjectInstance const &inst : object_instances) {
						*out = inst.object_transform;
						++out;
					}
				}

				workspace.copy_buffer(rtg, pipeline_name_to_index["A2ObjectsPipeline"], 1, needed_bytes);
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

						auto &cubemap_binding = texture_manager.environment_cubemap_binding;
						if (cubemap_binding && cubemap_binding->second != VK_NULL_HANDLE) {
							std::array< VkDescriptorSet, 2 > descriptor_sets{
								workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2BackgroundPipeline"]][0].descriptor, //0: World
								cubemap_binding->second, //1: Cubemap
							};

							vkCmdBindDescriptorSets(
								workspace.command_buffer, //command buffer
								VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
								background_pipeline.layout, //pipeline layout
								0, //set 0 and 1
								uint32_t(descriptor_sets.size()), descriptor_sets.data(), //descriptor sets count, ptr
								0, nullptr //dynamic offsets count, ptr
							);
						}

						vkCmdDraw(workspace.command_buffer, 36, 1, 0, 0); // hard coded for a cube
					}
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
							std::array< VkDescriptorSet, 2 > descriptor_sets{
								workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][0].descriptor, //0: World
								workspace.pipeline_buffer_pairs[pipeline_name_to_index["A2ObjectsPipeline"]][1].descriptor, //1: Transforms
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
						for (ObjectInstance const &inst : object_instances) {
							uint32_t index = uint32_t(&inst - &object_instances[0]);

							//bind texture descriptor set:
							auto &material_textures = texture_manager.texture_bindings_by_pipeline[pipeline_name_to_index["A2ObjectsPipeline"]][inst.material_index];
							auto &diffuse_binding_opt = material_textures[(TextureSlot::Diffuse)];
							if (!diffuse_binding_opt || !diffuse_binding_opt->texture || diffuse_binding_opt->descriptor_set == VK_NULL_HANDLE) continue;

							vkCmdBindDescriptorSets(
								workspace.command_buffer, //command buffer
								VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
								objects_pipeline.layout, //pipeline layout
								2, //set 2 (Diffuse texture)
								1, &diffuse_binding_opt->descriptor_set, //descriptor sets count, ptr
								0, nullptr //dynamic offsets count, ptr
							);

							// Bind cubemap descriptor set if available:
							auto &cubemap_binding = texture_manager.environment_cubemap_binding;
							if (!cubemap_binding || cubemap_binding->second == VK_NULL_HANDLE) continue;
							vkCmdBindDescriptorSets(
								workspace.command_buffer, //command buffer
								VK_PIPELINE_BIND_POINT_GRAPHICS, //pipeline bind point
								objects_pipeline.layout, //pipeline layout
								3, //set 3 (Cubemap)
								1, &cubemap_binding->second, //descriptor sets count, ptr
								0, nullptr //dynamic offsets count, ptr
							);
							vkCmdDraw(workspace.command_buffer, inst.object_ranges.count, 1, inst.object_ranges.first, index);
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

	{ //static sun and sky:
		object_world.SKY_DIRECTION.x = 0.0f;
		object_world.SKY_DIRECTION.y = 0.0f;
		object_world.SKY_DIRECTION.z = 1.0f;

		object_world.SKY_ENERGY.r = 0.1f;
		object_world.SKY_ENERGY.g = 0.1f;
		object_world.SKY_ENERGY.b = 0.2f;

		object_world.SUN_DIRECTION.x = 6.0f / 23.0f;
		object_world.SUN_DIRECTION.y = 13.0f / 23.0f;
		object_world.SUN_DIRECTION.z = 18.0f / 23.0f;

		object_world.SUN_ENERGY.r = 1.0f;
		object_world.SUN_ENERGY.g = 1.0f;
		object_world.SUN_ENERGY.b = 0.9f;
	}

	// Update camera
	camera_manager.update(dt, rtg.swapchain_extent.width, rtg.swapchain_extent.height);
	glm::mat4 PERSPECTIVE = camera_manager.get_perspective();
	glm::mat4 VIEW = camera_manager.get_view();

	{ // update background transform
		background_transform.PERSPECTIVE = PERSPECTIVE;
		background_transform.VIEW = VIEW;
	}

	{ // update object instances with frustum culling
		object_instances.clear();
		
		// Get frustum for culling
		auto frustum = camera_manager.get_frustum();

		for(uint32_t i = 0; i < doc->meshes.size(); ++i) 
		{
			const auto& mesh = doc->meshes[i];
			const auto& object_range = scene_manager.object_ranges[i];

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

				object_instances.emplace_back(ObjectInstance{
					.object_ranges = object_range,
					.object_transform{
						.PERSPECTIVE = PERSPECTIVE,
						.VIEW = VIEW,
						.MODEL = MODEL,
						.MODEL_NORMAL = MODEL_NORMAL,
					},
					.material_index = mesh.material_index.value_or(0),
				});
			}
		}
	}
}


void A2::on_input(InputEvent const &event) {
	camera_manager.on_input(event);
}
